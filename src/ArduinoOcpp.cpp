// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include "ArduinoOcpp.h"

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
#include <ArduinoOcpp/Core/FilesystemAdapter.h>

#include <ArduinoOcpp/MessagesV16/Authorize.h>
#include <ArduinoOcpp/MessagesV16/BootNotification.h>
#include <ArduinoOcpp/MessagesV16/StartTransaction.h>
#include <ArduinoOcpp/MessagesV16/StopTransaction.h>

#include <ArduinoOcpp/Debug.h>

namespace ArduinoOcpp {
namespace Facade {

#ifndef AO_CUSTOM_WS
WebSocketsClient *webSocket {nullptr};
OcppSocket *ocppSocket {nullptr};
#endif

OcppEngine *ocppEngine {nullptr};
std::shared_ptr<FilesystemAdapter> filesystem;
FilesystemOpt fileSystemOpt {};
float voltage_eff {230.f};

#define OCPP_NUMCONNECTORS 2
#define OCPP_ID_OF_CONNECTOR 1
#define OCPP_ID_OF_CP 0
bool OCPP_booted = false; //if BootNotification succeeded

} //end namespace ArduinoOcpp::Facade
} //end namespace ArduinoOcpp

using namespace ArduinoOcpp;
using namespace ArduinoOcpp::Facade;
using namespace ArduinoOcpp::Ocpp16;

#ifndef AO_CUSTOM_WS
void OCPP_initialize(const char *CS_hostname, uint16_t CS_port, const char *CS_url, float V_eff, ArduinoOcpp::FilesystemOpt fsOpt, ArduinoOcpp::OcppClock system_time) {
    if (ocppEngine) {
        AO_DBG_WARN("Can't be called two times. Either restart ESP, or call OCPP_deinitialize() before");
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
        AO_DBG_WARN("Can't be called two times. To change the credentials, either restart ESP, or call OCPP_deinitialize() before");
        return;
    }

    voltage_eff = V_eff;
    fileSystemOpt = fsOpt;

    filesystem = EspWiFi::makeDefaultFilesystemAdapter(fileSystemOpt);
    AO_DBG_DEBUG("filesystem %s", filesystem ? "loaded" : "error");
    
    configuration_init(filesystem); //call before each other library call

    ocppEngine = new OcppEngine(ocppSocket, system_time);
    auto& model = ocppEngine->getOcppModel();

    model.setChargePointStatusService(std::unique_ptr<ChargePointStatusService>(
        new ChargePointStatusService(*ocppEngine, OCPP_NUMCONNECTORS)));
    model.setHeartbeatService(std::unique_ptr<HeartbeatService>(
        new HeartbeatService(*ocppEngine)));

#if !defined(AO_CUSTOM_UPDATER) && !defined(AO_CUSTOM_WS)
    model.setFirmwareService(std::unique_ptr<FirmwareService>(
        EspWiFi::makeFirmwareService(*ocppEngine, "1234578901"))); //instantiate FW service + ESP installation routine
#else
    model.setFirmwareService(std::unique_ptr<FirmwareService>(
        new FirmwareService(*ocppEngine))); //only instantiate FW service
#endif

#if !defined(AO_CUSTOM_DIAGNOSTICS) && !defined(AO_CUSTOM_WS)
    model.setDiagnosticsService(std::unique_ptr<DiagnosticsService>(
        EspWiFi::makeDiagnosticsService(*ocppEngine))); //will only return "Rejected" because logging is not implemented yet
#else
    model.setDiagnosticsService(std::unique_ptr<DiagnosticsService>(
        new DiagnosticsService(*ocppEngine)));
#endif

#if !defined(AO_CUSTOM_RESET)
    model.getChargePointStatusService()->setExecuteReset(EspWiFi::makeDefaultResetFn());
#endif

    ocppEngine->setRunOcppTasks(false); //prevent OCPP classes from doing anything while booting
}

void OCPP_deinitialize() {
    AO_DBG_DEBUG("Still experimental function. If you find problems, it would be great if you publish them on the GitHub page");

    delete ocppEngine;
    ocppEngine = nullptr;

#ifndef AO_CUSTOM_WS
    delete ocppSocket;
    ocppSocket = nullptr;
    delete webSocket;
    webSocket = nullptr;
#endif

    simpleOcppFactory_deinitialize();

    fileSystemOpt = FilesystemOpt();
    voltage_eff = 230.f;

    OCPP_booted = false;
}

void OCPP_loop() {
    if (!ocppEngine) {
        AO_DBG_WARN("Please call OCPP_initialize before");
        //delay(200); //Prevent this message from flooding the Serial monitor.
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

}

void setPowerActiveImportSampler(std::function<float()> power) {
    if (!ocppEngine) {
        AO_DBG_ERR("Please call OCPP_initialize before");
        return;
    }

    auto& model = ocppEngine->getOcppModel();
    if (!model.getMeteringService()) {
        model.setMeteringSerivce(std::unique_ptr<MeteringService>(
            new MeteringService(*ocppEngine, OCPP_NUMCONNECTORS)));
    }
    SampledValueProperties meterProperties;
    meterProperties.setMeasurand("Power.Active.Import");
    meterProperties.setUnit("W");
    auto mvs = std::unique_ptr<SampledValueSamplerConcrete<int32_t, SampledValueDeSerializer<int32_t>>>(
                           new SampledValueSamplerConcrete<int32_t, SampledValueDeSerializer<int32_t>>(
            meterProperties,
            [power] (ReadingContext) {return power();}
    ));
    model.getMeteringService()->addMeterValueSampler(OCPP_ID_OF_CONNECTOR, std::move(mvs)); //connectorId=1
    model.getMeteringService()->setPowerSampler(OCPP_ID_OF_CONNECTOR, power);
}

void setEnergyActiveImportSampler(std::function<float()> energy) {
    if (!ocppEngine) {
        AO_DBG_ERR("Please call OCPP_initialize before");
        return;
    }
    auto& model = ocppEngine->getOcppModel();
    if (!model.getMeteringService()) {
        model.setMeteringSerivce(std::unique_ptr<MeteringService>(
            new MeteringService(*ocppEngine, OCPP_NUMCONNECTORS)));
    }
    SampledValueProperties meterProperties;
    meterProperties.setMeasurand("Energy.Active.Import.Register");
    meterProperties.setUnit("Wh");
    auto mvs = std::unique_ptr<SampledValueSamplerConcrete<int32_t, SampledValueDeSerializer<int32_t>>>(
                           new SampledValueSamplerConcrete<int32_t, SampledValueDeSerializer<int32_t>>(
            meterProperties,
            [energy] (ReadingContext) {return energy();}
    ));
    model.getMeteringService()->addMeterValueSampler(OCPP_ID_OF_CONNECTOR, std::move(mvs)); //connectorId=1
    model.getMeteringService()->setEnergySampler(OCPP_ID_OF_CONNECTOR, energy);
}

void addMeterValueSampler(std::unique_ptr<SampledValueSampler> meterValueSampler) {
    if (!ocppEngine) {
        AO_DBG_ERR("Please call OCPP_initialize before");
        return;
    }
    auto& model = ocppEngine->getOcppModel();
    if (!model.getMeteringService()) {
        model.setMeteringSerivce(std::unique_ptr<MeteringService>(
            new MeteringService(*ocppEngine, OCPP_NUMCONNECTORS)));
    }
    model.getMeteringService()->addMeterValueSampler(OCPP_ID_OF_CONNECTOR, std::move(meterValueSampler)); //connectorId=1
}

void setEvRequestsEnergySampler(std::function<bool()> evRequestsEnergy) {
    if (!ocppEngine) {
        AO_DBG_ERR("Please call OCPP_initialize before");
        return;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(OCPP_ID_OF_CONNECTOR);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->setEvRequestsEnergySampler(evRequestsEnergy);
}

void setConnectorEnergizedSampler(std::function<bool()> connectorEnergized) {
    if (!ocppEngine) {
        AO_DBG_ERR("Please call OCPP_initialize before");
        return;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(OCPP_ID_OF_CONNECTOR);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->setConnectorEnergizedSampler(connectorEnergized);
}

void setConnectorPluggedSampler(std::function<bool()> connectorPlugged) {
    if (!ocppEngine) {
        AO_DBG_ERR("Please call OCPP_initialize before");
        return;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(OCPP_ID_OF_CONNECTOR);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->setConnectorPluggedSampler(connectorPlugged);
}

void addConnectorErrorCodeSampler(std::function<const char *()> connectorErrorCode) {
    if (!ocppEngine) {
        AO_DBG_ERR("Please call OCPP_initialize before");
        return;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(OCPP_ID_OF_CONNECTOR);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->addConnectorErrorCodeSampler(connectorErrorCode);
}

void setOnChargingRateLimitChange(std::function<void(float)> chargingRateChanged) {
    if (!ocppEngine) {
        AO_DBG_ERR("Please call OCPP_initialize before");
        return;
    }
    auto& model = ocppEngine->getOcppModel();
    if (!model.getSmartChargingService()) {
        model.setSmartChargingService(std::unique_ptr<SmartChargingService>(
            new SmartChargingService(*ocppEngine, 11000.0f, voltage_eff, OCPP_NUMCONNECTORS, fileSystemOpt))); //default charging limit: 11kW
    }
    model.getSmartChargingService()->setOnLimitChange(chargingRateChanged);
}

void setOnUnlockConnector(std::function<PollResult<bool>()> unlockConnector) {
    if (!ocppEngine) {
        AO_DBG_ERR("Please call OCPP_initialize before");
        return;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(OCPP_ID_OF_CONNECTOR);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->setOnUnlockConnector(unlockConnector);
}

void setConnectorLock(std::function<ArduinoOcpp::TxEnableState(ArduinoOcpp::TxCondition)> lockConnector) {
    if (!ocppEngine) {
        AO_DBG_ERR("Please call OCPP_initialize before");
        return;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(OCPP_ID_OF_CONNECTOR);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->setConnectorLock(lockConnector);
}

void setTxBasedMeterUpdate(std::function<ArduinoOcpp::TxEnableState(ArduinoOcpp::TxCondition)> updateTxState) {
    if (!ocppEngine) {
        AO_DBG_ERR("Please call OCPP_initialize before");
        return;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(OCPP_ID_OF_CONNECTOR);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->setTxBasedMeterUpdate(updateTxState);
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

void authorize(const char *idTag, OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError, std::unique_ptr<Timeout> timeout) {
    if (!ocppEngine) {
        AO_DBG_ERR("Please call OCPP_initialize before");
        return;
    }
    if (!idTag || strnlen(idTag, IDTAG_LEN_MAX + 2) > IDTAG_LEN_MAX) {
        AO_DBG_ERR("idTag format violation. Expect c-style string with at most %u characters", IDTAG_LEN_MAX);
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

void bootNotification(const char *chargePointModel, const char *chargePointVendor, OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError, std::unique_ptr<Timeout> timeout) {
    if (!ocppEngine) {
        AO_DBG_ERR("Please call OCPP_initialize before");
        return;
    }
    
    auto credentials = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
        JSON_OBJECT_SIZE(2) + strlen(chargePointModel) + strlen(chargePointVendor) + 2));
    (*credentials)["chargePointModel"] = (char*) chargePointModel;
    (*credentials)["chargePointVendor"] = (char*) chargePointVendor;

    bootNotification(std::move(credentials), onConf, onAbort, onTimeout, onError, std::move(timeout));
}

void bootNotification(std::unique_ptr<DynamicJsonDocument> payload, OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError, std::unique_ptr<Timeout> timeout) {
    if (!ocppEngine) {
        AO_DBG_ERR("Please call OCPP_initialize before");
        return;
    }
    auto bootNotification = makeOcppOperation(
        new BootNotification(std::move(payload)));
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

void startTransaction(const char *idTag, OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError, std::unique_ptr<Timeout> timeout) {
    if (!ocppEngine) {
        AO_DBG_ERR("Please call OCPP_initialize before");
        return;
    }
    if (!idTag || strnlen(idTag, IDTAG_LEN_MAX + 2) > IDTAG_LEN_MAX) {
        AO_DBG_ERR("idTag format violation. Expect c-style string with at most %u characters", IDTAG_LEN_MAX);
        return;
    }
    auto startTransaction = makeOcppOperation(
        new StartTransaction(OCPP_ID_OF_CONNECTOR, idTag));
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

void stopTransaction(OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError, std::unique_ptr<Timeout> timeout) {
    if (!ocppEngine) {
        AO_DBG_ERR("Please call OCPP_initialize before");
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
        AO_DBG_WARN("Please call OCPP_initialize before");
        return -1;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(OCPP_ID_OF_CONNECTOR);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return -1;
    }
    return connector->getTransactionId();
}

bool ocppPermitsCharge() {
    if (!ocppEngine) {
        AO_DBG_WARN("Please call OCPP_initialize before");
        return false;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(OCPP_ID_OF_CONNECTOR);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return false;
    }
    return connector->ocppPermitsCharge();
}

bool isAvailable() {
    if (!ocppEngine) {
        AO_DBG_WARN("Please call OCPP_initialize before");
        return true; //assume "true" as default state
    }
    auto& model = ocppEngine->getOcppModel();
    auto chargePoint = model.getConnectorStatus(OCPP_ID_OF_CP);
    auto connector = model.getConnectorStatus(OCPP_ID_OF_CONNECTOR);
    if (!chargePoint || !connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return true; //assume "true" as default state
    }
    return (chargePoint->getAvailability() != AVAILABILITY_INOPERATIVE)
       &&  (connector->getAvailability() != AVAILABILITY_INOPERATIVE);
}

void beginSession(const char *idTag) {
    if (!ocppEngine) {
        AO_DBG_ERR("Please call OCPP_initialize before");
        return;
    }
    if (!idTag || strnlen(idTag, IDTAG_LEN_MAX + 2) > IDTAG_LEN_MAX) {
        AO_DBG_ERR("idTag format violation. Expect c-style string with at most %u characters", IDTAG_LEN_MAX);
        return;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(OCPP_ID_OF_CONNECTOR);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->beginSession(idTag);
}

void endSession() {
    if (!ocppEngine) {
        AO_DBG_ERR("Please call OCPP_initialize before");
        return;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(OCPP_ID_OF_CONNECTOR);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->endSession();
}

bool isInSession() {
    return getSessionIdTag() != nullptr;
}

const char *getSessionIdTag() {
    if (!ocppEngine) {
        AO_DBG_WARN("Please call OCPP_initialize before");
        return nullptr;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(OCPP_ID_OF_CONNECTOR);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return nullptr;
    }
    return connector->getSessionIdTag();
}

#if defined(AO_CUSTOM_UPDATER) || defined(AO_CUSTOM_WS)
ArduinoOcpp::FirmwareService *getFirmwareService() {
    auto& model = ocppEngine->getOcppModel();
    return model.getFirmwareService();
}
#endif

#if defined(AO_CUSTOM_DIAGNOSTICS) || defined(AO_CUSTOM_WS)
ArduinoOcpp::DiagnosticsService *getDiagnosticsService() {
    auto& model = ocppEngine->getOcppModel();
    return model.getDiagnosticsService();
}
#endif
