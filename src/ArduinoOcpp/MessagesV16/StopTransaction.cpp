// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/MessagesV16/StopTransaction.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Tasks/Metering/MeteringService.h>
#include <Variants.h>

using ArduinoOcpp::Ocpp16::StopTransaction;

StopTransaction::StopTransaction() {

}

StopTransaction::StopTransaction(int connectorId) : connectorId(connectorId) {

}

const char* StopTransaction::getOcppOperationType(){
    return "StopTransaction";
}

void StopTransaction::initiate() {

    if (getMeteringService() != NULL) {
        meterStop = getMeteringService()->readEnergyActiveImportRegister(connectorId);
    }

    OcppTime *ocppTime = getOcppTime();
    if (ocppTime) {
        otimestamp = ocppTime->getOcppTimestampNow();
    } else {
        otimestamp = MIN_TIME;
    }

    ConnectorStatus *connector = getConnectorStatus(connectorId);
    if (connector != NULL){
        connector->setTransactionId(-1); //immediate end of transaction
        connector->unauthorize();
    }

    if (DEBUG_OUT) Serial.println(F("[StartTransaction] StopTransaction initiated!"));

}

DynamicJsonDocument* StopTransaction::createReq() {

    DynamicJsonDocument *doc = new DynamicJsonDocument(JSON_OBJECT_SIZE(4) + (JSONDATE_LENGTH + 1));
    JsonObject payload = doc->to<JsonObject>();

    if (meterStop >= 0.f)
        payload["meterStop"] = meterStop; //TODO meterStart is required to be in Wh, but measuring unit is probably inconsistent in implementation

    if (otimestamp > MIN_TIME) {
        char timestamp[JSONDATE_LENGTH + 1] = {'\0'};
        otimestamp.toJsonString(timestamp, JSONDATE_LENGTH + 1);
        payload["timestamp"] = timestamp;
    }
    ConnectorStatus *connector = getConnectorStatus(connectorId);
    if (connector != NULL){
        payload["transactionId"] = connector->getTransactionIdSync();
        connector->setTransactionIdSync(-1);
    }

    return doc;
}

void StopTransaction::processConf(JsonObject payload) {

    //no need to process anything here

    if (DEBUG_OUT) Serial.print(F("[StopTransaction] Request has been accepted!\n"));
    
}


void StopTransaction::processReq(JsonObject payload) {
    /**
     * Ignore Contents of this Req-message, because this is for debug purposes only
     */
}

DynamicJsonDocument* StopTransaction::createConf(){
    DynamicJsonDocument* doc = new DynamicJsonDocument(2 * JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();

    JsonObject idTagInfo = payload.createNestedObject("idTagInfo");
    idTagInfo["status"] = "Accepted";

    return doc;
}
