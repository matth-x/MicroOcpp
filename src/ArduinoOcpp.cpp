// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include "ArduinoOcpp.h"

#include "Variants.h"

#if USE_FACADE

#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Core/OcppSocket.h>
#include <ArduinoOcpp/Tasks/Metering/MeteringService.h>
#include <ArduinoOcpp/Tasks/SmartCharging/SmartChargingService.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>
#include <ArduinoOcpp/Core/Configuration.h>

#include <ArduinoOcpp/MessagesV16/Authorize.h>
#include <ArduinoOcpp/MessagesV16/BootNotification.h>
#include <ArduinoOcpp/MessagesV16/StartTransaction.h>
#include <ArduinoOcpp/MessagesV16/StopTransaction.h>
#include <ArduinoOcpp/Core/OcppOperationTimeout.h>

namespace ArduinoOcpp {
namespace Facade {

#ifndef AO_CUSTOM_WS
WebSocketsClient webSocket;
#endif
OcppSocket *ocppSocket;

MeteringService *meteringService;
PowerSampler powerSampler;
EnergySampler energySampler;
std::function<bool()> evRequestsEnergySampler = NULL; //bool (*evRequestsEnergySampler)() = NULL;
bool evRequestsEnergyLastState = false;
std::function<bool()> connectorEnergizedSampler = NULL;
bool connectorEnergizedLastState = false;
SmartChargingService *smartChargingService;
ChargePointStatusService *chargePointStatusService;
OnLimitChange onLimitChange;
OcppTime *ocppTime;

#define OCPP_NUMCONNECTORS 2
#define OCPP_ID_OF_CONNECTOR 1
#define OCPP_ID_OF_CP 0
boolean OCPP_initialized = false;
boolean OCPP_booted = false; //if BootNotification succeeded

#if 0 //moved to OcppConnection
/*
   Called by Websocket library on incoming message on the internet link
*/
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

    switch (type) {
        case WStype_DISCONNECTED:
            Serial.print(F("[WSc] Disconnected!\n"));
            break;
        case WStype_CONNECTED:
            Serial.printf("[WSc] Connected to url: %s\n", payload);
            break;
        case WStype_TEXT:
            if (DEBUG_OUT || TRAFFIC_OUT) Serial.printf("[WSc] get text: %s\n", payload);

            if (!processWebSocketEvent((const char *) payload, length)) { //forward message to OcppEngine
                Serial.print(F("[WSc] Processing WebSocket input event failed!\n"));
            }
            break;
        case WStype_FRAGMENT_TEXT_START: //fragments are not supported
            Serial.print(F("[WSc] Fragments are not supported\n"));
            if (!processWebSocketUnsupportedEvent((const char *) payload, length)) { //forward message to OcppEngine
                Serial.print(F("[WSc] Processing WebSocket input event failed!\n"));
            }
            break;
        case WStype_BIN:
            Serial.print(F("[WSc] Incoming binary data stream not supported"));
            break;
        case WStype_PING:
            // pong will be send automatically
            Serial.print(F("[WSc] get ping\n"));
            break;
        case WStype_PONG:
            // answer to a ping we send
            Serial.print(F("[WSc] get pong\n"));
            break;
        default:
            Serial.print(F("[WSc] Unsupported WebSocket event type\n"));
            break;
    }
}

#endif //moved to OcppConnection

} //end namespace ArduinoOcpp::Facade
} //end namespace ArduinoOcpp

using namespace ArduinoOcpp;
using namespace ArduinoOcpp::Facade;
using namespace ArduinoOcpp::Ocpp16;

#ifndef AO_CUSTOM_WS
void OCPP_initialize(String CS_hostname, uint16_t CS_port, String CS_url, float V_eff, ArduinoOcpp::FilesystemOpt fsOpt, ArduinoOcpp::OcppClock system_time) {
    if (OCPP_initialized) {
        Serial.print(F("[ArduinoOcpp] Error: cannot call OCPP_initialize() two times! If you want to reconfigure the library, please restart your ESP\n"));
        return;
    }

    // server address, port and URL
    webSocket.begin(CS_hostname, CS_port, CS_url, "ocpp1.6");

    // event handler
    //webSocket.onEvent(webSocketEvent); //will be set in ocppEngine_initialize()

    // use HTTP Basic Authorization this is optional remove if not needed
    // webSocket.setAuthorization("user", "Password");

    // try ever 5000 again if connection has failed
    webSocket.setReconnectInterval(5000);

    // start heartbeat (optional)
    // ping server every 15000 ms
    // expect pong from server within 3000 ms
    // consider connection disconnected if pong is not received 2 times
    webSocket.enableHeartbeat(15000, 3000, 2); //comment this one out to for specific OCPP servers

    ocppSocket = new EspWiFi::OcppClientSocket(&webSocket);

    OCPP_initialize(ocppSocket, V_eff, fsOpt);
}
#endif

void OCPP_initialize(OcppSocket *ocppSocket, float V_eff, ArduinoOcpp::FilesystemOpt fsOpt, ArduinoOcpp::OcppClock system_time) {
    if (OCPP_initialized) {
        Serial.print(F("[ArduinoOcpp] Error: cannot call OCPP_initialize() two times! If you want to reconfigure the library, please restart your ESP\n"));
        return;
    }

    if (!ocppSocket) {
        Serial.print(F("[ArduinoOcpp] OCPP_initialize(ocppSocket): ocppSocket cannot be NULL!\n"));
        return;
    }
    
    configuration_init(fsOpt); //call before each other library call

    ocppEngine_initialize(ocppSocket);

    ocppTime = new OcppTime(system_time);
    ocppEngine_setOcppTime(ocppTime);

    smartChargingService = new SmartChargingService(11000.0f, V_eff, OCPP_NUMCONNECTORS, ocppTime, fsOpt); //default charging limit: 11kW
    chargePointStatusService = new ChargePointStatusService(OCPP_NUMCONNECTORS, ocppTime); //Constructor adds instance to ocppEngine in constructor
    meteringService = new MeteringService(OCPP_NUMCONNECTORS, ocppTime);

    OCPP_initialized = true;
}

void OCPP_loop() {
    if (!OCPP_initialized) {
        Serial.print(F("[ArduinoOcpp] Error: you must call OCPP_initialize before calling the loop() function!\n"));
        delay(200); //Prevent this error message from flooding the Serial monitor.
        return;
    }

    //webSocket.loop();                 //moved to Core/OcppSocket
    ocppEngine_loop();                  //mandatory

    if (!OCPP_booted) {
        if (chargePointStatusService->isBooted()) {
            OCPP_booted = true;
        } else {
            return; //wait until the first BootNotification succeeded
        }
    }

    if (onLimitChange != NULL) {
        smartChargingService->loop();   //optional
    }

    chargePointStatusService->loop();   //optional

    if (powerSampler != NULL || energySampler != NULL) {
        meteringService->loop();        //optional
    }

    bool evRequestsEnergyNewState = true;
    if (evRequestsEnergySampler != NULL) {
        evRequestsEnergyNewState = evRequestsEnergySampler();
    } else {
        if (powerSampler != NULL) {
            evRequestsEnergyNewState = powerSampler() >= 5.f;
        }
    }

    if (!evRequestsEnergyLastState && evRequestsEnergyNewState) {
        evRequestsEnergyLastState = true;
        chargePointStatusService->getConnector(OCPP_ID_OF_CONNECTOR)->startEvDrawsEnergy();
    } else if (evRequestsEnergyLastState && !evRequestsEnergyNewState) {
        evRequestsEnergyLastState = false;
        chargePointStatusService->getConnector(OCPP_ID_OF_CONNECTOR)->stopEvDrawsEnergy();
    }

    bool connectorEnergizedNewState = true;
    if (connectorEnergizedSampler != NULL) {
        connectorEnergizedNewState = connectorEnergizedSampler();
    } else {
        connectorEnergizedNewState = getTransactionId() >= 0;
    }

    if (!connectorEnergizedLastState && connectorEnergizedNewState) {
        connectorEnergizedLastState = true;
        chargePointStatusService->getConnector(OCPP_ID_OF_CONNECTOR)->startEnergyOffer();
    } else if (connectorEnergizedLastState && !connectorEnergizedNewState) {
        connectorEnergizedLastState = false;
        chargePointStatusService->getConnector(OCPP_ID_OF_CONNECTOR)->stopEnergyOffer();
    }

}

void setPowerActiveImportSampler(std::function<float()> power) {
    powerSampler = power;
    meteringService->setPowerSampler(OCPP_ID_OF_CONNECTOR, powerSampler); //connectorId=1
}

void setEnergyActiveImportSampler(std::function<float()> energy) {
    energySampler = energy;
    meteringService->setEnergySampler(OCPP_ID_OF_CONNECTOR, energySampler); //connectorId=1
}

void setEvRequestsEnergySampler(std::function<bool()> evRequestsEnergy) {
    evRequestsEnergySampler = evRequestsEnergy;
}

void setConnectorEnergizedSampler(std::function<bool()> connectorEnergized) {
    connectorEnergizedSampler = connectorEnergized;
}

void setOnChargingRateLimitChange(std::function<void(float)> chargingRateChanged) {
    onLimitChange = chargingRateChanged;
    smartChargingService->setOnLimitChange(onLimitChange);
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

void authorize(String &idTag, OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError, Timeout *timeout) {
    OcppOperation *authorize = makeOcppOperation(
        new Authorize(idTag));
    initiateOcppOperation(authorize);
    if (onConf)
        authorize->setOnReceiveConfListener(onConf);
    if (onAbort)
        authorize->setOnAbortListener(onAbort);
    if (onTimeout)
        authorize->setOnTimeoutListener(onTimeout);
    if (onError)
        authorize->setOnReceiveErrorListener(onError);
    if (timeout)
        authorize->setTimeout(timeout);
    else
        authorize->setTimeout(new FixedTimeout(20000));
}

void bootNotification(String chargePointModel, String chargePointVendor, OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError, Timeout *timeout) {
    OcppOperation *bootNotification = makeOcppOperation(
        new BootNotification(chargePointModel, chargePointVendor));
    initiateOcppOperation(bootNotification);
    if (onConf)
        bootNotification->setOnReceiveConfListener(onConf);
    if (onAbort)
        bootNotification->setOnAbortListener(onAbort);
    if (onTimeout)
        bootNotification->setOnTimeoutListener(onTimeout);
    if (onError)
        bootNotification->setOnReceiveErrorListener(onError);
    if (timeout)
        bootNotification->setTimeout(timeout);
    else
        bootNotification->setTimeout(new SuppressedTimeout());
}

void bootNotification(String &chargePointModel, String &chargePointVendor, String &chargePointSerialNumber, OnReceiveConfListener onConf) {
    OcppOperation *bootNotification = makeOcppOperation(
        new BootNotification(chargePointModel, chargePointVendor, chargePointSerialNumber));
    initiateOcppOperation(bootNotification);
    bootNotification->setOnReceiveConfListener(onConf);
    bootNotification->setTimeout(new SuppressedTimeout());
}

void startTransaction(OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError, Timeout *timeout) {
    OcppOperation *startTransaction = makeOcppOperation(
        new StartTransaction(OCPP_ID_OF_CONNECTOR));
    initiateOcppOperation(startTransaction);
    if (onConf)
        startTransaction->setOnReceiveConfListener(onConf);
    if (onAbort)
        startTransaction->setOnAbortListener(onAbort);
    if (onTimeout)
        startTransaction->setOnTimeoutListener(onTimeout);
    if (onError)
        startTransaction->setOnReceiveErrorListener(onError);
    if (timeout)
        startTransaction->setTimeout(timeout);
    else
        startTransaction->setTimeout(new SuppressedTimeout());
}

void startTransaction(String &idTag, OnReceiveConfListener onConf) {
    OcppOperation *startTransaction = makeOcppOperation(
        new StartTransaction(OCPP_ID_OF_CONNECTOR, idTag));
    initiateOcppOperation(startTransaction);
    startTransaction->setOnReceiveConfListener(onConf);
    startTransaction->setTimeout(new SuppressedTimeout());
}

void stopTransaction(OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError, Timeout *timeout) {
    OcppOperation *stopTransaction = makeOcppOperation(
        new StopTransaction(OCPP_ID_OF_CONNECTOR));
    initiateOcppOperation(stopTransaction);
    if (onConf)
        stopTransaction->setOnReceiveConfListener(onConf);
    if (onAbort)
        stopTransaction->setOnAbortListener(onAbort);
    if (onTimeout)
        stopTransaction->setOnTimeoutListener(onTimeout);
    if (onError)
        stopTransaction->setOnReceiveErrorListener(onError);
    if (timeout)
        stopTransaction->setTimeout(timeout);
    else
        stopTransaction->setTimeout(new SuppressedTimeout());
}

//void startEvDrawsEnergy() {
//    chargePointStatusService->getConnector(OCPP_ID_OF_CONNECTOR)->startEvDrawsEnergy();
//}

//void stopEvDrawsEnergy() {
//    chargePointStatusService->getConnector(OCPP_ID_OF_CONNECTOR)->stopEvDrawsEnergy();
//}

int getTransactionId() {
    return chargePointStatusService->getConnector(OCPP_ID_OF_CONNECTOR)->getTransactionId();
}

bool existsUnboundIdTag() {
    return chargePointStatusService->existsUnboundAuthorization();
}

#endif
