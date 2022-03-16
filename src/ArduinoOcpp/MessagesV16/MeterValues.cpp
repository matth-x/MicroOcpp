// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/MessagesV16/MeterValues.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Tasks/Metering/MeterValue.h>
#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::Ocpp16::MeterValues;

//can only be used for echo server debugging
MeterValues::MeterValues() {
    
}

MeterValues::MeterValues(const std::vector<std::unique_ptr<MeterValue>>& meterValue, int connectorId, int transactionId) 
      : connectorId{connectorId}, transactionId{transactionId} {

    for (auto value = meterValue.begin(); value != meterValue.end(); value++) {
        this->meterValue.push_back(std::unique_ptr<MeterValue>(new MeterValue(**value)));
    }
}

MeterValues::~MeterValues(){

}

const char* MeterValues::getOcppOperationType(){
    return "MeterValues";
}

std::unique_ptr<DynamicJsonDocument> MeterValues::createReq() {

    size_t capacity = 0;
    
    std::vector<std::unique_ptr<DynamicJsonDocument>> entries;
    for (auto value = meterValue.begin(); value != meterValue.end(); value++) {
        auto entry = (*value)->toJson();
        capacity += entry->capacity();
        entries.push_back(std::move(entry));
    }

    capacity += JSON_OBJECT_SIZE(3);
    capacity += JSON_ARRAY_SIZE(entries.size());

    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(capacity + 100)); //TODO remove safety space
    auto payload = doc->to<JsonObject>();
    payload["connectorId"] = connectorId;

    if (ocppModel && ocppModel->getConnectorStatus(connectorId)) {
        auto connector = ocppModel->getConnectorStatus(connectorId);
        if (connector->getTransactionIdSync() >= 0) {
            payload["transactionId"] = connector->getTransactionIdSync();
        }
    }

    auto meterValueJson = payload.createNestedArray("meterValue");
    for (auto entry = entries.begin(); entry != entries.end(); entry++) {
        meterValueJson.add(**entry);
    }

    return doc;
}

void MeterValues::processConf(JsonObject payload) {
    AO_DBG_DEBUG("Request has been confirmed");
}


void MeterValues::processReq(JsonObject payload) {

    /**
     * Ignore Contents of this Req-message, because this is for debug purposes only
     */

}

std::unique_ptr<DynamicJsonDocument> MeterValues::createConf(){
    return createEmptyDocument();
}
