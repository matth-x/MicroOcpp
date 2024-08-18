// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/MeterValues.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Metering/MeterValue.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::MeterValues;
using MicroOcpp::JsonDoc;
using MicroOcpp::Transaction;

//can only be used for echo server debugging
MeterValues::MeterValues(Model& model) : MemoryManaged("v16.Operation.", "MeterValues"), model(model) {
    
}

MeterValues::MeterValues(Model& model, std::unique_ptr<MeterValue> meterValue, unsigned int connectorId, std::shared_ptr<Transaction> transaction) 
      : MemoryManaged("v16.Operation.", "MeterValues"), model(model), meterValue{std::move(meterValue)}, connectorId{connectorId}, transaction{transaction} {
    
}

MeterValues::~MeterValues(){

}

const char* MeterValues::getOperationType(){
    return "MeterValues";
}

std::unique_ptr<JsonDoc> MeterValues::createReq() {

    size_t capacity = 0;

    std::unique_ptr<JsonDoc> meterValueJson;

    if (meterValue) {

        if (meterValue->getTimestamp() < MIN_TIME) {
            MO_DBG_DEBUG("adjust preboot MeterValue timestamp");
            Timestamp adjusted = model.getClock().adjustPrebootTimestamp(meterValue->getTimestamp());
            meterValue->setTimestamp(adjusted);
        }

        meterValueJson = meterValue->toJson();
        if (meterValueJson) {
            capacity += meterValueJson->capacity();
        } else {
            MO_DBG_ERR("Energy meter reading not convertible to JSON");
        }
    }

    capacity += JSON_OBJECT_SIZE(3);
    capacity += JSON_ARRAY_SIZE(1);

    auto doc = makeJsonDoc(getMemoryTag(), capacity);
    auto payload = doc->to<JsonObject>();
    payload["connectorId"] = connectorId;

    if (transaction && transaction->getTransactionId() > 0) { //add txId if MVs are assigned to a tx with txId
        payload["transactionId"] = transaction->getTransactionId();
    }

    auto meterValueArray = payload.createNestedArray("meterValue");
    if (meterValueJson) {
        meterValueArray.add(*meterValueJson);
    }

    return doc;
}

void MeterValues::processConf(JsonObject payload) {
    MO_DBG_DEBUG("Request has been confirmed");
}

std::shared_ptr<Transaction>& MeterValues::getTransaction() {
    return transaction;
}

unsigned int MeterValues::getOpNr() {
    if (!meterValue) {
        return 1;
    }
    return meterValue->getOpNr();
}


void MeterValues::processReq(JsonObject payload) {

    /**
     * Ignore Contents of this Req-message, because this is for debug purposes only
     */

}

std::unique_ptr<JsonDoc> MeterValues::createConf(){
    return createEmptyDocument();
}
