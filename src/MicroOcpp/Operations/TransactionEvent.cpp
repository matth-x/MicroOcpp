// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Operations/TransactionEvent.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp::Ocpp201;
using MicroOcpp::JsonDoc;

TransactionEvent::TransactionEvent(Model& model, TransactionEventData *txEvent)
        : MemoryManaged("v201.Operation.", "TransactionEvent"), model(model), txEvent(txEvent) {

}

const char* TransactionEvent::getOperationType() {
    return "TransactionEvent";
}

std::unique_ptr<JsonDoc> TransactionEvent::createReq() {

    size_t capacity = 0;

    if (txEvent->eventType == TransactionEventData::Type::Ended) {
        for (size_t i = 0; i < txEvent->transaction->sampledDataTxEnded.size(); i++) {
            JsonDoc meterValueJson = initJsonDoc(getMemoryTag()); //just measure, create again for serialization later
            txEvent->transaction->sampledDataTxEnded[i]->toJson(meterValueJson);
            capacity += meterValueJson.capacity();
        }
    }

    for (size_t i = 0; i < txEvent->meterValue.size(); i++) {
        JsonDoc meterValueJson = initJsonDoc(getMemoryTag()); //just measure, create again for serialization later
        txEvent->meterValue[i]->toJson(meterValueJson);
        capacity += meterValueJson.capacity();
    }

    capacity +=
            JSON_OBJECT_SIZE(12) + //total of 12 fields
            JSONDATE_LENGTH + 1 + //timestamp string
            JSON_OBJECT_SIZE(5) + //transactionInfo
                MO_TXID_LEN_MAX + 1 + //transactionId
            MO_IDTOKEN_LEN_MAX + 1; //idToken

    auto doc = makeJsonDoc(getMemoryTag(), capacity);
    JsonObject payload = doc->to<JsonObject>();

    payload["eventType"] = serializeTransactionEventType(txEvent->eventType);

    char timestamp [JSONDATE_LENGTH + 1];
    txEvent->timestamp.toJsonString(timestamp, JSONDATE_LENGTH + 1);
    payload["timestamp"] = timestamp;

    if (serializeTransactionEventTriggerReason(txEvent->triggerReason)) {
        payload["triggerReason"] = serializeTransactionEventTriggerReason(txEvent->triggerReason);
    } else {
        MO_DBG_ERR("serialization error");
    }

    payload["seqNo"] = txEvent->seqNo;

    if (txEvent->offline) {
        payload["offline"] = txEvent->offline;
    }

    if (txEvent->numberOfPhasesUsed >= 0) {
        payload["numberOfPhasesUsed"] = txEvent->numberOfPhasesUsed;
    }

    if (txEvent->cableMaxCurrent >= 0) {
        payload["cableMaxCurrent"] = txEvent->cableMaxCurrent;
    }

    if (txEvent->reservationId >= 0) {
        payload["reservationId"] = txEvent->reservationId;
    }

    JsonObject transactionInfo = payload.createNestedObject("transactionInfo");
    transactionInfo["transactionId"] = txEvent->transaction->transactionId;

    if (serializeTransactionEventChargingState(txEvent->chargingState)) { // optional
        transactionInfo["chargingState"] = serializeTransactionEventChargingState(txEvent->chargingState);
    }

    if (txEvent->transaction->stoppedReason != Transaction::StoppedReason::Local &&
            serializeTransactionStoppedReason(txEvent->transaction->stoppedReason)) { // optional
        transactionInfo["stoppedReason"] = serializeTransactionStoppedReason(txEvent->transaction->stoppedReason);
    }

    if (txEvent->remoteStartId >= 0) {
        transactionInfo["remoteStartId"] = txEvent->transaction->remoteStartId;
    }

    if (txEvent->idToken) {
        JsonObject idToken = payload.createNestedObject("idToken");
        idToken["idToken"] = txEvent->idToken->get();
        idToken["type"] = txEvent->idToken->getTypeCstr();
    }

    if (txEvent->evse.id >= 0) {
        JsonObject evse = payload.createNestedObject("evse");
        evse["id"] = txEvent->evse.id;
        if (txEvent->evse.connectorId >= 0) {
            evse["connectorId"] = txEvent->evse.connectorId;
        }
    }

    if (txEvent->eventType == TransactionEventData::Type::Ended) {
        for (size_t i = 0; i < txEvent->transaction->sampledDataTxEnded.size(); i++) {
            JsonDoc meterValueJson = initJsonDoc(getMemoryTag());
            txEvent->transaction->sampledDataTxEnded[i]->toJson(meterValueJson);
            payload["meterValue"].add(meterValueJson);
        }
    }

    for (size_t i = 0; i < txEvent->meterValue.size(); i++) {
        JsonDoc meterValueJson = initJsonDoc(getMemoryTag());
        txEvent->meterValue[i]->toJson(meterValueJson);
        payload["meterValue"].add(meterValueJson);
    }

    return doc;
}

void TransactionEvent::processConf(JsonObject payload) {

    if (payload.containsKey("idTokenInfo")) {
        const char *status = payload["idTokenInfo"]["status"] | "_Undefined";
        if (strcmp(status, "Accepted")) {
            MO_DBG_INFO("transaction deAuthorized");
            txEvent->transaction->active = false;
            txEvent->transaction->isDeauthorized = true;
        }
    }
}

void TransactionEvent::processReq(JsonObject payload) {
    /**
     * Ignore Contents of this Req-message, because this is for debug purposes only
     */
}

std::unique_ptr<JsonDoc> TransactionEvent::createConf() {
    return createEmptyDocument();
}

#endif // MO_ENABLE_V201
