// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include "ArduinoOcpp.h"

#include "Variants.h"

#if USE_FACADE

#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/Metering/MeteringService.h>
#include <ArduinoOcpp/Tasks/SmartCharging/SmartChargingService.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Tasks/Heartbeat/HeartbeatService.h>
#include <ArduinoOcpp/Tasks/FirmwareManagement/FirmwareService.h>
#include <ArduinoOcpp/Tasks/Diagnostics/DiagnosticsService.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>
#include <ArduinoOcpp/Core/Configuration.h>

#include <ArduinoOcpp/MessagesV16/Authorize.h>
#include <ArduinoOcpp/MessagesV16/BootNotification.h>
#include <ArduinoOcpp/MessagesV16/StartTransaction.h>
#include <ArduinoOcpp/MessagesV16/StopTransaction.h>

namespace ArduinoOcpp {
namespace Facade {

#ifndef AO_CUSTOM_WS
WebSocketsClient *webSocket {nullptr};
OcppSocket *ocppSocket {nullptr};
#endif

PowerSampler powerSampler {nullptr};
std::function<bool()> evRequestsEnergySampler {nullptr};
bool evRequestsEnergyLastState {false};
std::function<bool()> connectorEnergizedSampler {nullptr};
bool connectorEnergizedLastState {false};

OcppEngine *ocppEngine {nullptr};
FilesystemOpt fileSystemOpt {};
float voltage_eff {230.f};

#define OCPP_NUMCONNECTORS 2
#define OCPP_ID_OF_CONNECTOR 1
#define OCPP_ID_OF_CP 0
boolean OCPP_booted = false; //if BootNotification succeeded

} //end namespace ArduinoOcpp::Facade
} //end namespace ArduinoOcpp

using namespace ArduinoOcpp;
using namespace ArduinoOcpp::Facade;
using namespace ArduinoOcpp::Ocpp16;

#ifndef AO_CUSTOM_WS
void OCPP_initialize(String CS_hostname, uint16_t CS_port, String CS_url, float V_eff, ArduinoOcpp::FilesystemOpt fsOpt, ArduinoOcpp::OcppClock system_time) {
    if (ocppEngine) {
        Serial.print(F("[ArduinoOcpp] Error: cannot call OCPP_initialize() two times! If you want to reconfigure the library, please restart your ESP\n"));
        return;
    }

    if (!webSocket)
        webSocket = new WebSocketsClient();

    // server address, port and URL
    webSocket->begin(CS_hostname, CS_port, CS_url, "ocpp1.6");

    // try ever 5000 again if connection has failed
    webSocket->setReconnectInterval(5000);

    // start heartbeat (optional)
    // ping server every 15000 ms
    // expect pong from server within 3000 ms
    // consider connection disconnected if pong is not received 2 times
    webSocket->enableHeartbeat(15000, 3000, 2); //comment this one out to for specific OCPP servers

    delete ocppSocket;
    ocppSocket = new EspWiFi::OcppClientSocket(webSocket);

    OCPP_initialize(*ocppSocket, V_eff, fsOpt);
}
#endif

void OCPP_initialize(OcppSocket& ocppSocket, float V_eff, ArduinoOcpp::FilesystemOpt fsOpt, ArduinoOcpp::OcppClock system_time) {
    if (ocppEngine) {
        Serial.print(F("[ArduinoOcpp] Error: cannot call OCPP_initialize() two times! If you want to reconfigure the library, please restart your ESP\n"));
        return;
    }

    voltage_eff = V_eff;
    fileSystemOpt = fsOpt;
    
    configuration_init(fileSystemOpt); //call before each other library call

    ocppEngine = new OcppEngine(ocppSocket, system_time);
    auto& model = ocppEngine->getOcppModel();

    model.setChargePointStatusService(std::unique_ptr<ChargePointStatusService>(
        new ChargePointStatusService(*ocppEngine, OCPP_NUMCONNECTORS)));
    model.setHeartbeatService(std::unique_ptr<HeartbeatService>(
        new HeartbeatService(*ocppEngine)));

#if !defined(AO_CUSTOM_UPDATER) && !defined(AO_CUSTOM_WEBSOCKET)
    model.setFirmwareService(std::unique_ptr<FirmwareService>(
        EspWiFi::makeFirmwareService(*ocppEngine, "1234578901"))); //instantiate FW service + ESP installation routine
#else
    model.setFirmwareService(std::unique_ptr<FirmwareService>(
        new FirmwareService(*ocppEngine))); //only instantiate FW service
#endif

#if !defined(AO_CUSTOM_DIAGNOSTICS) && !defined(AO_CUSTOM_WEBSOCKET)
    model.setDiagnosticsService(std::unique_ptr<DiagnosticsService>(
        EspWiFi::makeDiagnosticsService(*ocppEngine))); //will only return "Rejected" because logging is not implemented yet
#else
    model.setDiagnosticsService(std::unique_ptr<DiagnosticsService>(
        new DiagnosticsService(*ocppEngine)));
#endif

    ocppEngine->setRunOcppTasks(false); //prevent OCPP classes from doing anything while booting
}

void OCPP_deinitialize() {
    Serial.println(F("[ArduinoOcpp] called OCPP_deinitialize: still experimental function. If you find problems, it would be great if you publish them on the GitHub page"));

    delete ocppEngine;
    ocppEngine = nullptr;

#ifndef AO_CUSTOM_WS
    delete ocppSocket;
    ocppSocket = nullptr;
    delete webSocket;
    webSocket = nullptr;
#endif

    simpleOcppFactory_deinitialize();
    
    powerSampler = nullptr;
    evRequestsEnergySampler = nullptr;
    evRequestsEnergyLastState = false;
    connectorEnergizedSampler = nullptr;
    connectorEnergizedLastState = false;

    fileSystemOpt = FilesystemOpt();
    voltage_eff = 230.f;

    OCPP_booted = false;
}

void OCPP_loop() {
    if (!ocppEngine) {
        Serial.print(F("[ArduinoOcpp] Error: you must call OCPP_initialize before calling the loop() function!\n"));
        delay(200); //Prevent this error message from flooding the Serial monitor.
        return;
    }

    ocppEngine->loop();

    auto& model = ocppEngine->getOcppModel();

    if (!OCPP_booted) {
        auto csService = model.getChargePointStatusService();
        if (!csService || csService->isBooted()) {
            OCPP_booted = true;
            ocppEngine->setRunOcppTasks(true);
        } else {
            return; //wait until the first BootNotification succeeded
        }
    }

    bool evRequestsEnergyNewState = true;
    if (evRequestsEnergySampler != nullptr) {
        evRequestsEnergyNewState = evRequestsEnergySampler();
    } else {
        if (powerSampler != nullptr) {
            evRequestsEnergyNewState = powerSampler() >= 50.f;
        }
    }

    if (!evRequestsEnergyLastState && evRequestsEnergyNewState) {
        evRequestsEnergyLastState = true;
        if (model.getConnectorStatus(OCPP_ID_OF_CONNECTOR))
            model.getConnectorStatus(OCPP_ID_OF_CONNECTOR)->startEvDrawsEnergy();
    } else if (evRequestsEnergyLastState && !evRequestsEnergyNewState) {
        evRequestsEnergyLastState = false;
        if (model.getConnectorStatus(OCPP_ID_OF_CONNECTOR))
            model.getConnectorStatus(OCPP_ID_OF_CONNECTOR)->stopEvDrawsEnergy();
    }

    bool connectorEnergizedNewState = true;
    if (connectorEnergizedSampler != nullptr) {
        connectorEnergizedNewState = connectorEnergizedSampler();
    } else {
        connectorEnergizedNewState = getTransactionId() >= 0;
    }

    if (!connectorEnergizedLastState && connectorEnergizedNewState) {
        connectorEnergizedLastState = true;
        if (model.getConnectorStatus(OCPP_ID_OF_CONNECTOR))
            model.getConnectorStatus(OCPP_ID_OF_CONNECTOR)->startEnergyOffer();
    } else if (connectorEnergizedLastState && !connectorEnergizedNewState) {
        connectorEnergizedLastState = false;
        if (model.getConnectorStatus(OCPP_ID_OF_CONNECTOR))
            model.getConnectorStatus(OCPP_ID_OF_CONNECTOR)->stopEnergyOffer();
    }

}

void setPowerActiveImportSampler(std::function<float()> power) {
    if (!ocppEngine) {
        Serial.println(F("[ArduinoOcpp.cpp] in setPowerActiveImportSampler(): You must call OCPP_initialize() before. Ignore"));
        return;
    }
    powerSampler = power;
    auto& model = ocppEngine->getOcppModel();
    if (!model.getMeteringService()) {
        model.setMeteringSerivce(std::unique_ptr<MeteringService>(
            new MeteringService(*ocppEngine, OCPP_NUMCONNECTORS)));
    }
    model.getMeteringService()->setPowerSampler(OCPP_ID_OF_CONNECTOR, powerSampler); //connectorId=1
}

void setEnergyActiveImportSampler(std::function<float()> energy) {
    if (!ocppEngine) {
        Serial.println(F("[ArduinoOcpp.cpp] in setEnergyActiveImportSampler(): You must call OCPP_initialize() before. Ignore"));
        return;
    }
    auto& model = ocppEngine->getOcppModel();
    if (!model.getMeteringService()) {
        model.setMeteringSerivce(std::unique_ptr<MeteringService>(
            new MeteringService(*ocppEngine, OCPP_NUMCONNECTORS)));
    }
    model.getMeteringService()->setEnergySampler(OCPP_ID_OF_CONNECTOR, energy); //connectorId=1
}

void setEvRequestsEnergySampler(std::function<bool()> evRequestsEnergy) {
    evRequestsEnergySampler = evRequestsEnergy;
}

void setConnectorEnergizedSampler(std::function<bool()> connectorEnergized) {
    connectorEnergizedSampler = connectorEnergized;
}

void setConnectorPluggedSampler(std::function<bool()> connectorPlugged) {
    if (!ocppEngine) {
        Serial.println(F("[ArduinoOcpp.cpp] in setConnectorPluggedSampler(): You must call OCPP_initialize() before. Ignore"));
        return;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(OCPP_ID_OF_CONNECTOR);
    if (!connector) {
        Serial.println(F("[ArduinoOcpp.cpp] in setConnectorPluggedSampler(): Could not find connector. Ignore"));
        return;
    }
    connector->setConnectorPluggedSampler(connectorPlugged);
}

void addConnectorErrorCodeSampler(std::function<const char *()> connectorErrorCode) {
    if (!ocppEngine) {
        Serial.println(F("[ArduinoOcpp.cpp] in addConnectorErrorCodeSampler(): You must call OCPP_initialize() before. Ignore"));
        return;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(OCPP_ID_OF_CONNECTOR);
    if (!connector) {
        Serial.println(F("[ArduinoOcpp.cpp] in addConnectorErrorCodeSampler(): Could not find connector. Ignore"));
        return;
    }
    connector->addConnectorErrorCodeSampler(connectorErrorCode);
}

void setOnChargingRateLimitChange(std::function<void(float)> chargingRateChanged) {
    if (!ocppEngine) {
        Serial.println(F("[ArduinoOcpp.cpp] in setOnChargingRateLimitChange(): You must call OCPP_initialize() before. Ignore"));
        return;
    }
    auto& model = ocppEngine->getOcppModel();
    if (!model.getSmartChargingService()) {
        model.setSmartChargingService(std::unique_ptr<SmartChargingService>(
            new SmartChargingService(*ocppEngine, 11000.0f, voltage_eff, OCPP_NUMCONNECTORS, fileSystemOpt))); //default charging limit: 11kW
    }
    model.getSmartChargingService()->setOnLimitChange(chargingRateChanged);
}

void setOnUnlockConnector(std::function<bool()> unlockConnector) {
    if (!ocppEngine) {
        Serial.println(F("[ArduinoOcpp.cpp] in setOnUnlockConnector(): You must call OCPP_initialize() before. Ignore"));
        return;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(OCPP_ID_OF_CONNECTOR);
    if (!connector) {
        Serial.println(F("[ArduinoOcpp.cpp] in setOnUnlockConnector(): Could not find connector. Ignore"));
        return;
    }
    connector->setOnUnlockConnector(unlockConnector);
}

void setOnSetChargingProfileRequest(OnReceiveReqListener onReceiveReq) {
     setOnSetChargingProfileRequestListener(onReceiveReq);
}

void setOnRemoteStartTransactionSendConf(OnSendConfListener onSendConf) {
     setOnRemoteStartTransactionSendConfListener(onSendConf);
}

void setOnRemoteStopTransactionReceiveReq(OnReceiveReqListener onReceiveReq) {
     setOnRemoteStopTransactionReceiveRequestListener(onReceiveReq);
}

void setOnRemoteStopTransactionSendConf(OnSendConfListener onSendConf) {
     setOnRemoteStopTransactionSendConfListener(onSendConf);
}

void setOnResetSendConf(OnSendConfListener onSendConf) {
     setOnResetSendConfListener(onSendConf);
}

void setOnResetReceiveReq(OnReceiveReqListener onReceiveReq) {
     setOnResetReceiveRequestListener(onReceiveReq);
}

void authorize(String &idTag, OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError, std::unique_ptr<Timeout> timeout) {
    if (!ocppEngine) {
        Serial.println(F("[ArduinoOcpp.cpp] in authorize(): You must call OCPP_initialize() before. Ignore"));
        return;
    }
    auto authorize = makeOcppOperation(
        new Authorize(idTag));
    if (onConf)
        authorize->setOnReceiveConfListener(onConf);
    if (onAbort)
        authorize->setOnAbortListener(onAbort);
    if (onTimeout)
        authorize->setOnTimeoutListener(onTimeout);
    if (onError)
        authorize->setOnReceiveErrorListener(onError);
    if (timeout)
        authorize->setTimeout(std::move(timeout));
    else
        authorize->setTimeout(std::unique_ptr<Timeout>(new FixedTimeout(20000)));
    ocppEngine->initiateOperation(std::move(authorize));
}

void bootNotification(String chargePointModel, String chargePointVendor, OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError, std::unique_ptr<Timeout> timeout) {
    if (!ocppEngine) {
        Serial.println(F("[ArduinoOcpp.cpp] in bootNotification(): You must call OCPP_initialize() before. Ignore"));
        return;
    }
    auto bootNotification = makeOcppOperation(
        new BootNotification(chargePointModel, chargePointVendor));
    if (onConf)
        bootNotification->setOnReceiveConfListener(onConf);
    if (onAbort)
        bootNotification->setOnAbortListener(onAbort);
    if (onTimeout)
        bootNotification->setOnTimeoutListener(onTimeout);
    if (onError)
        bootNotification->setOnReceiveErrorListener(onError);
    if (timeout)
        bootNotification->setTimeout(std::move(timeout));
    else
        bootNotification->setTimeout(std::unique_ptr<Timeout> (new SuppressedTimeout()));
    ocppEngine->initiateOperation(std::move(bootNotification));
}

void bootNotification(String &chargePointModel, String &chargePointVendor, String &chargePointSerialNumber, OnReceiveConfListener onConf) {
    if (!ocppEngine) {
        Serial.println(F("[ArduinoOcpp.cpp] in bootNotification(): You must call OCPP_initialize() before. Ignore"));
        return;
    }
    auto bootNotification = makeOcppOperation(
        new BootNotification(chargePointModel, chargePointVendor, chargePointSerialNumber));
    bootNotification->setOnReceiveConfListener(onConf);
    bootNotification->setTimeout(std::unique_ptr<Timeout> (new SuppressedTimeout()));
    ocppEngine->initiateOperation(std::move(bootNotification));
}

void bootNotification(DynamicJsonDocument *payload, OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError, std::unique_ptr<Timeout> timeout) {
    if (!ocppEngine) {
        Serial.println(F("[ArduinoOcpp.cpp] in bootNotification(): You must call OCPP_initialize() before. Ignore"));
        return;
    }
    auto bootNotification = makeOcppOperation(
        new BootNotification(payload));
    if (onConf)
        bootNotification->setOnReceiveConfListener(onConf);
    if (onAbort)
        bootNotification->setOnAbortListener(onAbort);
    if (onTimeout)
        bootNotification->setOnTimeoutListener(onTimeout);
    if (onError)
        bootNotification->setOnReceiveErrorListener(onError);
    if (timeout)
        bootNotification->setTimeout(std::move(timeout));
    else
        bootNotification->setTimeout(std::unique_ptr<Timeout>(new SuppressedTimeout()));
    ocppEngine->initiateOperation(std::move(bootNotification));
}

void startTransaction(OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError, std::unique_ptr<Timeout> timeout) {
    if (!ocppEngine) {
        Serial.println(F("[ArduinoOcpp.cpp] in startTransaction(): You must call OCPP_initialize() before. Ignore"));
        return;
    }
    auto startTransaction = makeOcppOperation(
        new StartTransaction(OCPP_ID_OF_CONNECTOR));
    if (onConf)
        startTransaction->setOnReceiveConfListener(onConf);
    if (onAbort)
        startTransaction->setOnAbortListener(onAbort);
    if (onTimeout)
        startTransaction->setOnTimeoutListener(onTimeout);
    if (onError)
        startTransaction->setOnReceiveErrorListener(onError);
    if (timeout)
        startTransaction->setTimeout(std::move(timeout));
    else
        startTransaction->setTimeout(std::unique_ptr<Timeout>(new SuppressedTimeout()));
    ocppEngine->initiateOperation(std::move(startTransaction));
}

void startTransaction(String &idTag, OnReceiveConfListener onConf) {
    if (!ocppEngine) {
        Serial.println(F("[ArduinoOcpp.cpp] in startTransaction(): You must call OCPP_initialize() before. Ignore"));
        return;
    }
    auto startTransaction = makeOcppOperation(
        new StartTransaction(OCPP_ID_OF_CONNECTOR, idTag));
    startTransaction->setOnReceiveConfListener(onConf);
    startTransaction->setTimeout(std::unique_ptr<Timeout>(new SuppressedTimeout()));
    ocppEngine->initiateOperation(std::move(startTransaction));
}

void stopTransaction(OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError, std::unique_ptr<Timeout> timeout) {
    if (!ocppEngine) {
        Serial.println(F("[ArduinoOcpp.cpp] in stopTransaction(): You must call OCPP_initialize() before. Ignore"));
        return;
    }
    auto stopTransaction = makeOcppOperation(
        new StopTransaction(OCPP_ID_OF_CONNECTOR));
    if (onConf)
        stopTransaction->setOnReceiveConfListener(onConf);
    if (onAbort)
        stopTransaction->setOnAbortListener(onAbort);
    if (onTimeout)
        stopTransaction->setOnTimeoutListener(onTimeout);
    if (onError)
        stopTransaction->setOnReceiveErrorListener(onError);
    if (timeout)
        stopTransaction->setTimeout(std::move(timeout));
    else
        stopTransaction->setTimeout(std::unique_ptr<Timeout>(new SuppressedTimeout()));
    ocppEngine->initiateOperation(std::move(stopTransaction));
}

int getTransactionId() {
    if (!ocppEngine) {
        Serial.println(F("[ArduinoOcpp.cpp] in getTransactionId(): You must call OCPP_initialize() before"));
        return -1;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(OCPP_ID_OF_CONNECTOR);
    if (!connector) {
        Serial.println(F("[ArduinoOcpp.cpp] in getTransactionId(): Could not find connector"));
        return -1;
    }
    return connector->getTransactionId();
}

bool existsUnboundIdTag() {
    if (!ocppEngine) {
        Serial.println(F("[ArduinoOcpp.cpp] in existsUnboundIdTag(): You must call OCPP_initialize() before"));
        return false;
    }
    auto csService = ocppEngine->getOcppModel().getChargePointStatusService();
    if (!csService) {
        Serial.println(F("[ArduinoOcpp.cpp] in existsUnboundIdTag(): Could not find connector"));
        return false;
    }
    return csService->existsUnboundAuthorization();
}

bool isAvailable() {
    if (!ocppEngine) {
        Serial.println(F("[ArduinoOcpp.cpp] in isAvailable(): You must call OCPP_initialize() before"));
        return true; //assume "true" as default state
    }
    auto& model = ocppEngine->getOcppModel();
    auto chargePoint = model.getConnectorStatus(OCPP_ID_OF_CP);
    auto connector = model.getConnectorStatus(OCPP_ID_OF_CONNECTOR);
    if (!chargePoint || !connector) {
        Serial.println(F("[ArduinoOcpp.cpp] in isAvailable(): Could not find connector"));
        return true; //assume "true" as default state
    }
    return (chargePoint->getAvailability() != AVAILABILITY_INOPERATIVE)
       &&  (connector->getAvailability() != AVAILABILITY_INOPERATIVE);
}

#endif
