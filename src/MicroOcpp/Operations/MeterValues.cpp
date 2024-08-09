// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/MeterValues.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Metering/MeterValue.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::MeterValues;
using MicroOcpp::MemJsonDoc;

#define ENERGY_METER_TIMEOUT_MS 30 * 1000  //after waiting for 30s, send MeterValues without missing readings

//can only be used for echo server debugging
MeterValues::MeterValues() : AllocOverrider("v16.Operation.", getOperationType()) {
    
}

MeterValues::MeterValues(MemVector<std::unique_ptr<MeterValue>>&& meterValue, unsigned int connectorId, std::shared_ptr<Transaction> transaction) 
      : meterValue{std::move(meterValue)}, connectorId{connectorId}, transaction{transaction} {
    
}

MeterValues::~MeterValues(){

}

const char* MeterValues::getOperationType(){
    return "MeterValues";
}

std::unique_ptr<MemJsonDoc> MeterValues::createReq() {

    size_t capacity = 0;
    
    auto entries = makeMemVector<std::unique_ptr<MemJsonDoc>>(getMemoryTag());
    for (auto value = meterValue.begin(); value != meterValue.end(); value++) {
        auto entry = (*value)->toJson();
        if (entry) {
            capacity += entry->capacity();
            entries.push_back(std::move(entry));
        } else {
            MO_DBG_ERR("Energy meter reading not convertible to JSON");
        }
    }

    capacity += JSON_OBJECT_SIZE(3);
    capacity += JSON_ARRAY_SIZE(entries.size());

    auto doc = makeMemJsonDoc(capacity + 100, getMemoryTag()); //TODO remove safety space
    auto payload = doc->to<JsonObject>();
    payload["connectorId"] = connectorId;

    if (transaction && transaction->getTransactionId() > 0) { //add txId if MVs are assigned to a tx with txId
        payload["transactionId"] = transaction->getTransactionId();
    }

    auto meterValueJson = payload.createNestedArray("meterValue");
    for (auto entry = entries.begin(); entry != entries.end(); entry++) {
        meterValueJson.add(**entry);
    }

    return doc;
}

void MeterValues::processConf(JsonObject payload) {
    MO_DBG_DEBUG("Request has been confirmed");
}


void MeterValues::processReq(JsonObject payload) {

    /**
     * Ignore Contents of this Req-message, because this is for debug purposes only
     */

}

std::unique_ptr<MemJsonDoc> MeterValues::createConf(){
    return createEmptyDocument();
}
