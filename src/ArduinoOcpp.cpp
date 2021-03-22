// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include "ArduinoOcpp.h"

#include "Variants.h"

#if USE_FACADE

#include "OcppEngine.h"
#include "MeteringService.h"
#include "SmartChargingService.h"
#include "ChargePointStatusService.h"
#include "SimpleOcppOperationFactory.h"
#include "Configuration.h"

#include "Authorize.h"
#include "BootNotification.h"
#include "StartTransaction.h"
#include "StopTransaction.h"
#include "OcppOperationTimeout.h"

namespace ArduinoOcpp {
namespace Facade {

WebSocketsClient webSocket;

MeteringService *meteringService;
PowerSampler powerSampler;
EnergySampler energySampler;
bool (*evRequestsEnergySampler)() = NULL;
bool evRequestsEnergyLastState = false;
SmartChargingService *smartChargingService;
ChargePointStatusService *chargePointStatusService;
OnLimitChange onLimitChange;

#define OCPP_NUMCONNECTORS 2
#define OCPP_ID_OF_CONNECTOR 1
#define OCPP_ID_OF_CP 0
boolean OCPP_initialized = false;

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

} //end namespace ArduinoOcpp::Facade
} //end namespace ArduinoOcpp

using namespace ArduinoOcpp::Facade;

void OCPP_initialize(String CS_hostname, uint16_t CS_port, String CS_url) {
    if (OCPP_initialized) {
        Serial.print(F("[SingleConnectorEvseFacade] Error: cannot call OCPP_initialize() two times! If you want to reconfigure the library, please restart your ESP\n"));
        return;
    }

    configuration_init(); //call before each other library call

    // server address, port and URL
    webSocket.begin(CS_hostname, CS_port, CS_url, "ocpp1.6");

    // event handler
    webSocket.onEvent(webSocketEvent);

    // use HTTP Basic Authorization this is optional remove if not needed
    // webSocket.setAuthorization("user", "Password");

    // try ever 5000 again if connection has failed
    webSocket.setReconnectInterval(5000);

    // start heartbeat (optional)
    // ping server every 15000 ms
    // expect pong from server within 3000 ms
    // consider connection disconnected if pong is not received 2 times
    webSocket.enableHeartbeat(15000, 3000, 2); //comment this one out to for specific OCPP servers

    ocppEngine_initialize(&webSocket, 2048); //default JSON document size = 2048

    smartChargingService = new SmartChargingService(16.0f, OCPP_NUMCONNECTORS); //default charging limit: 16A
    chargePointStatusService = new ChargePointStatusService(&webSocket, OCPP_NUMCONNECTORS); //Constructor adds instance to ocppEngine in constructor
    meteringService = new MeteringService(&webSocket, OCPP_NUMCONNECTORS);

    OCPP_initialized = true;
}

void OCPP_loop() {
    if (!OCPP_initialized) {
        Serial.print(F("[SingleConnectorEvseFacade] Error: you must call OCPP_initialize before calling the loop() function!\n"));
        delay(200); //Prevent this error message from flooding the Serial monitor.
        return;
    }

    webSocket.loop();                   //mandatory
    ocppEngine_loop();                  //mandatory

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

}

void setPowerActiveImportSampler(float power()) {
    powerSampler = power;
    meteringService->setPowerSampler(OCPP_ID_OF_CONNECTOR, powerSampler); //connectorId=1
}

void setEnergyActiveImportSampler(float energy()) {
    energySampler = energy;
    meteringService->setEnergySampler(OCPP_ID_OF_CONNECTOR, energySampler); //connectorId=1
}

void setEvRequestsEnergySampler(bool evRequestsEnergy()) {
    evRequestsEnergySampler = evRequestsEnergy;

}

void setOnChargingRateLimitChange(void chargingRateChanged(float limit)) {
    onLimitChange = chargingRateChanged;
    smartChargingService->setOnLimitChange(onLimitChange);
}

void setOnSetChargingProfileRequest(void listener(JsonObject payload)) {
     setOnSetChargingProfileRequestListener(listener);
}

void setOnRemoteStartTransactionSendConf(void listener(JsonObject payload)) {
     setOnRemoteStartTransactionSendConfListener(listener);
}

void setOnRemoteStopTransactionSendConf(void listener(JsonObject payload)) {
     setOnRemoteStopTransactionSendConfListener(listener);
}

void setOnResetSendConf(void listener(JsonObject payload)) {
     setOnResetSendConfListener(listener);
}

void authorize(String &idTag, OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError) {
    OcppOperation *authorize = makeOcppOperation(&webSocket,
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
    authorize->setTimeout(new FixedTimeout(20000));
}

void bootNotification(String chargePointModel, String chargePointVendor, OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError) {
    OcppOperation *bootNotification = makeOcppOperation(&webSocket,
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
    bootNotification->setTimeout(new SuppressedTimeout());
}

void bootNotification(String &chargePointModel, String &chargePointVendor, String &chargePointSerialNumber, OnReceiveConfListener onConf) {
    OcppOperation *bootNotification = makeOcppOperation(&webSocket,
        new BootNotification(chargePointModel, chargePointVendor, chargePointSerialNumber));
    initiateOcppOperation(bootNotification);
    bootNotification->setOnReceiveConfListener(onConf);
    bootNotification->setTimeout(new SuppressedTimeout());
}

void startTransaction(OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError) {
    OcppOperation *startTransaction = makeOcppOperation(&webSocket,
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
    startTransaction->setTimeout(new FixedTimeout(20000));
}

void startTransaction(String &idTag, OnReceiveConfListener onConf) {
    OcppOperation *startTransaction = makeOcppOperation(&webSocket,
        new StartTransaction(OCPP_ID_OF_CONNECTOR, idTag));
    initiateOcppOperation(startTransaction);
    startTransaction->setOnReceiveConfListener(onConf);
    startTransaction->setTimeout(new FixedTimeout(20000));
}

void stopTransaction(OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError) {
    OcppOperation *stopTransaction = makeOcppOperation(&webSocket,
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

#endif
