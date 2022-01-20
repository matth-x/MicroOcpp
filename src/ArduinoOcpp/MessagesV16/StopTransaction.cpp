// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/MessagesV16/StopTransaction.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
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

    if (ocppModel && ocppModel->getMeteringService()) {
        auto meteringService = ocppModel->getMeteringService();
        meterStop = meteringService->readEnergyActiveImportRegister(connectorId);
    }

    if (ocppModel) {
        otimestamp = ocppModel->getOcppTime().getOcppTimestampNow();
    } else {
        otimestamp = MIN_TIME;
    }

    if (ocppModel && ocppModel->getConnectorStatus(connectorId)){
        auto connector = ocppModel->getConnectorStatus(connectorId);
        connector->setTransactionId(-1); //immediate end of transaction
        connector->unauthorize();
    }

    if (DEBUG_OUT) Serial.println(F("[StartTransaction] StopTransaction initiated!"));
}

std::unique_ptr<DynamicJsonDocument> StopTransaction::createReq() {
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(4) + (JSONDATE_LENGTH + 1)));
    JsonObject payload = doc->to<JsonObject>();

    if (meterStop >= 0.f)
        payload["meterStop"] = meterStop; //TODO meterStart is required to be in Wh, but measuring unit is probably inconsistent in implementation

    if (otimestamp > MIN_TIME) {
        char timestamp[JSONDATE_LENGTH + 1] = {'\0'};
        otimestamp.toJsonString(timestamp, JSONDATE_LENGTH + 1);
        payload["timestamp"] = timestamp;
    }
    
    if (ocppModel && ocppModel->getConnectorStatus(connectorId)){
        auto connector = ocppModel->getConnectorStatus(connectorId);
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

std::unique_ptr<DynamicJsonDocument> StopTransaction::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(2 * JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();

    JsonObject idTagInfo = payload.createNestedObject("idTagInfo");
    idTagInfo["status"] = "Accepted";

    return doc;
}
