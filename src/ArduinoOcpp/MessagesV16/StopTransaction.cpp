// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/MessagesV16/StopTransaction.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Tasks/Metering/MeteringService.h>
#include <ArduinoOcpp/Tasks/Metering/MeterValue.h>
#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::Ocpp16::StopTransaction;

#define ENERGY_METER_TIMEOUT_MS 60 * 1000  //after waiting for 60s, send StopTx without start reading

StopTransaction::StopTransaction(int connectorId, const char *reason) : connectorId(connectorId) {
    if (reason) {
        snprintf(this->reason, REASON_LEN_MAX, "%s", reason);
    }
}

const char* StopTransaction::getOcppOperationType(){
    return "StopTransaction";
}

void StopTransaction::initiate() {

    if (ocppModel && ocppModel->getMeteringService()) {
        auto meteringService = ocppModel->getMeteringService();
        meterStop = meteringService->readTxEnergyMeter(connectorId, ReadingContext::TransactionEnd);
        transactionData = meteringService->createStopTxMeterData(connectorId);
    }

    if (ocppModel) {
        otimestamp = ocppModel->getOcppTime().getOcppTimestampNow();
    } else {
        otimestamp = MIN_TIME;
    }

    if (ocppModel && ocppModel->getConnectorStatus(connectorId)){
        auto connector = ocppModel->getConnectorStatus(connectorId);
        connector->setTransactionId(-1); //immediate end of transaction
        if (connector->getSessionIdTag()) {
            AO_DBG_DEBUG("Ending EV user session triggered by StopTransaction");
            connector->endSession();
        }
    }

    emTimeout = ao_tick_ms();

    AO_DBG_INFO("StopTransaction initiated!");
}

std::unique_ptr<DynamicJsonDocument> StopTransaction::createReq() {

    if (meterStop && !*meterStop) {
        //meterStop not ready yet
        if (ao_tick_ms() - emTimeout < ENERGY_METER_TIMEOUT_MS) {
            return nullptr;
        }
    }

    std::vector<std::unique_ptr<DynamicJsonDocument>> txDataJson;
    size_t txDataJson_size = 0;
    for (auto mv = transactionData.begin(); mv != transactionData.end(); mv++) {
        auto mvJson = (*mv)->toJson();
        if (!mvJson) {
            return nullptr;
        }
        txDataJson_size += mvJson->capacity();
        txDataJson.emplace_back(std::move(mvJson));
    }

    DynamicJsonDocument txDataDoc = DynamicJsonDocument(JSON_ARRAY_SIZE(txDataJson.size()) + txDataJson_size);
    for (auto mvJson = txDataJson.begin(); mvJson != txDataJson.end(); mvJson++) {
        txDataDoc.add(**mvJson);
    }

    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
                JSON_OBJECT_SIZE(6) + //total of 6 fields
                (JSONDATE_LENGTH + 1) + //timestamp string
                (REASON_LEN_MAX + 1) + //reason string
                txDataDoc.capacity()));
    JsonObject payload = doc->to<JsonObject>();

    if (meterStop && *meterStop) {
        payload["meterStop"] = meterStop->toInteger();
    } else {
        AO_DBG_ERR("Energy meter timeout");
        payload["meterStart"] = -1;
    }

    if (otimestamp > MIN_TIME) {
        char timestamp[JSONDATE_LENGTH + 1] = {'\0'};
        otimestamp.toJsonString(timestamp, JSONDATE_LENGTH + 1);
        payload["timestamp"] = timestamp;
    }
    
    if (ocppModel && ocppModel->getConnectorStatus(connectorId)){
        auto connector = ocppModel->getConnectorStatus(connectorId);
        payload["transactionId"] = connector->getTransactionIdSync();
    }

    if (reason[0] != '\0') {
        payload["reason"] = reason;
    }

    if (!transactionData.empty()) {
        payload["transactionData"] = txDataDoc;
    }

    return doc;
}

void StopTransaction::processConf(JsonObject payload) {

    if (ocppModel && ocppModel->getConnectorStatus(connectorId)){
        auto connector = ocppModel->getConnectorStatus(connectorId);
        connector->setTransactionIdSync(-1);
    }

    AO_DBG_INFO("Request has been accepted!");
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
