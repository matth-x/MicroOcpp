// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/TransactionEvent.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V201

using namespace MicroOcpp;
using namespace MicroOcpp::Ocpp201;

TransactionEvent::TransactionEvent(Context& context, TransactionEventData *txEvent)
        : MemoryManaged("v201.Operation.", "TransactionEvent"), context(context), txEvent(txEvent) {

}

const char* TransactionEvent::getOperationType() {
    return "TransactionEvent";
}

std::unique_ptr<JsonDoc> TransactionEvent::createReq() {

    size_t capacity = 0;

    if (txEvent->eventType == TransactionEventData::Type::Ended) {
        for (size_t i = 0; i < txEvent->transaction->sampledDataTxEnded.size(); i++) {
            capacity += txEvent->transaction->sampledDataTxEnded[i]->getJsonCapacity(MO_OCPP_V201, /* internalFormat */ false);
        }
    }

    for (size_t i = 0; i < txEvent->meterValue.size(); i++) {
        capacity += txEvent->meterValue[i]->getJsonCapacity(MO_OCPP_V201, /* internalFormat */ false);
    }

    capacity +=
            JSON_OBJECT_SIZE(12) + //total of 12 fields
            MO_JSONDATE_SIZE +     //timestamp string
            JSON_OBJECT_SIZE(5);   //transactionInfo

    auto doc = makeJsonDoc(getMemoryTag(), capacity);
    JsonObject payload = doc->to<JsonObject>();

    payload["eventType"] = serializeTransactionEventType(txEvent->eventType);

    char timestamp [MO_JSONDATE_SIZE];
    if (!context.getClock().toJsonString(txEvent->timestamp, timestamp, sizeof(timestamp))) {
        MO_DBG_ERR("internal error");
        timestamp[0] = '\0';
    }
    payload["timestamp"] = timestamp;

    if (serializeTxEventTriggerReason(txEvent->triggerReason)) {
        payload["triggerReason"] = serializeTxEventTriggerReason(txEvent->triggerReason);
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
    transactionInfo["transactionId"] = (const char*)txEvent->transaction->transactionId; //force zero-copy

    if (serializeTransactionEventChargingState(txEvent->chargingState)) { // optional
        transactionInfo["chargingState"] = serializeTransactionEventChargingState(txEvent->chargingState);
    }

    if (txEvent->transaction->stoppedReason != MO_TxStoppedReason_Local &&
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
            JsonObject meterValueJson = payload["meterValue"].createNestedObject();
            txEvent->transaction->sampledDataTxEnded[i]->toJson(context.getClock(), MO_OCPP_V201, /*internal format*/ false, meterValueJson);
        }
    }

    for (size_t i = 0; i < txEvent->meterValue.size(); i++) {
        JsonObject meterValueJson = payload["meterValue"].createNestedObject();
        txEvent->meterValue[i]->toJson(context.getClock(), MO_OCPP_V201, /*internal format*/ false, meterValueJson);
    }

    return doc;
}

void TransactionEvent::processConf(JsonObject payload) {

    if (payload.containsKey("idTokenInfo")) {
        if (strcmp(payload["idTokenInfo"]["status"], "Accepted")) {
            MO_DBG_INFO("transaction deAuthorized");
            txEvent->transaction->active = false;
            txEvent->transaction->isDeauthorized = true;
        }
    }
}

#if MO_ENABLE_MOCK_SERVER
void TransactionEvent::onRequestMock(const char *operationType, const char *payloadJson, void **userStatus, void *userData) {
    //if request contains `"idToken"`, then send response with `"idTokenInfo"`. Instead of building the
    //full JSON DOM, just search for the substring
    if (strstr(payloadJson, "\"idToken\"")) {
        //found, pass status to `writeMockConf`
        *userStatus = reinterpret_cast<void*>(1);
    } else {
        //not found, pass status to `writeMockConf`
        *userStatus = reinterpret_cast<void*>(0);
    }
}

int TransactionEvent::writeMockConf(const char *operationType, char *buf, size_t size, void *userStatus, void *userData) {
    (void)userData;

    if (userStatus == reinterpret_cast<void*>(1)) {
        return snprintf(buf, size, "{\"idTokenInfo\":{\"status\":\"Accepted\"}}");
    } else {
        return snprintf(buf, size, "{}");
    }
}
#endif //MO_ENABLE_MOCK_SERVER

#endif //MO_ENABLE_V201
