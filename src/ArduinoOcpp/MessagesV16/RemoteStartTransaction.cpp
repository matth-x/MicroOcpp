// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License


#include <ArduinoOcpp/MessagesV16/RemoteStartTransaction.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <Variants.h>

using ArduinoOcpp::Ocpp16::RemoteStartTransaction;

RemoteStartTransaction::RemoteStartTransaction() {
  
}

const char* RemoteStartTransaction::getOcppOperationType() {
    return "RemoteStartTransaction";
}

void RemoteStartTransaction::processReq(JsonObject payload) {
    connectorId = payload["connectorId"] | -1;

    if (payload.containsKey("idTag")) {
        String idTag = payload["idTag"] | String("Invalid");
        ChargePointStatusService *cpService = getChargePointStatusService();
        if (cpService != NULL) { 
            cpService->authorize(idTag);
        }
    }
}

DynamicJsonDocument* RemoteStartTransaction::createConf(){
    DynamicJsonDocument* doc = new DynamicJsonDocument(JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    
    bool canStartTransaction = false;
    if (connectorId >= 1) {
        //connectorId specified for given connector, try to start Transaction here
        ConnectorStatus *connector = getConnectorStatus(connectorId);
        if (connector != NULL){
            if (connector->getTransactionId() < 0) {
                canStartTransaction = true;
            }
        }
    } else {
        //connectorId not specified. Find free connector
        if (getChargePointStatusService() != NULL) {
            for (int i = 1; i < getChargePointStatusService()->getNumConnectors(); i++) {
                ConnectorStatus *connIter = getConnectorStatus(i);
                if (connIter->getTransactionId() < 0) {
                canStartTransaction = true; 
                }
            }
        }
    }

    if (canStartTransaction){
        payload["status"] = "Accepted";
    } else {
        payload["status"] = "Rejected";
    }
    
    return doc;
}

DynamicJsonDocument* RemoteStartTransaction::createReq() {
    DynamicJsonDocument *doc = new DynamicJsonDocument(JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();

    payload["idTag"] = "A0-00-00-00";

    return doc;
}

void RemoteStartTransaction::processConf(JsonObject payload){
    String status = payload["status"] | "Invalid";

    if (status.equals("Accepted")) {
        if (DEBUG_OUT) Serial.print(F("[RemoteStartTransaction] Request has been accepted!\n"));
    } else {
        Serial.print(F("[RemoteStartTransaction] Request has been denied!"));
    }
}
