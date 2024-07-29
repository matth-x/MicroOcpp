// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/StopTransaction.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Authorization/AuthorizationService.h>
#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Model/Metering/MeterValue.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Debug.h>
#include <MicroOcpp/Version.h>

using MicroOcpp::Ocpp16::StopTransaction;

StopTransaction::StopTransaction(Model& model, std::shared_ptr<Transaction> transaction)
        : model(model), transaction(transaction) {

}

StopTransaction::StopTransaction(Model& model, std::shared_ptr<Transaction> transaction, std::vector<std::unique_ptr<MicroOcpp::MeterValue>> transactionData)
        : model(model), transaction(transaction), transactionData(std::move(transactionData)) {

}

const char* StopTransaction::getOperationType() {
    return "StopTransaction";
}

std::unique_ptr<DynamicJsonDocument> StopTransaction::createReq() {

    /*
     * Adjust timestamps in case they were taken before initial Clock setting
     */
    if (transaction->getStopTimestamp() < MIN_TIME) {
        //Timestamp taken before Clock value defined. Determine timestamp
        if (transaction->getStopBootNr() == model.getBootNr()) {
            //possible to calculate real timestamp
            Timestamp adjusted = model.getClock().adjustPrebootTimestamp(transaction->getStopTimestamp());
            transaction->setStopTimestamp(adjusted);
        } else if (transaction->getStartTimestamp() >= MIN_TIME) {
            MO_DBG_WARN("set stopTime = startTime because correct time is not available");
            transaction->setStopTimestamp(transaction->getStartTimestamp() + 1); //1s behind startTime to keep order in backend DB
        } else {
            MO_DBG_ERR("failed to determine StopTx timestamp");
            //send invalid value
        }
    }

    // if StopTx timestamp is before StartTx timestamp, something probably went wrong. Restore reasonable temporal order
    if (transaction->getStopTimestamp() < transaction->getStartTimestamp()) {
        MO_DBG_WARN("set stopTime = startTime because stopTime was before startTime");
        transaction->setStopTimestamp(transaction->getStartTimestamp() + 1); //1s behind startTime to keep order in backend DB
    }

    for (auto mv = transactionData.begin(); mv != transactionData.end(); mv++) {
        if ((*mv)->getTimestamp() < MIN_TIME) {
            //time off. Try to adjust, otherwise send invalid value
            if ((*mv)->getReadingContext() == ReadingContext::TransactionBegin) {
                (*mv)->setTimestamp(transaction->getStartTimestamp());
            } else if ((*mv)->getReadingContext() == ReadingContext::TransactionEnd) {
                (*mv)->setTimestamp(transaction->getStopTimestamp());
            } else {
                (*mv)->setTimestamp(transaction->getStartTimestamp() + 1);
            }
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
                (IDTAG_LEN_MAX + 1) + //stop idTag
                (JSONDATE_LENGTH + 1) + //timestamp string
                (REASON_LEN_MAX + 1) + //reason string
                txDataDoc.capacity()));
    JsonObject payload = doc->to<JsonObject>();

    if (transaction->getStopIdTag() && *transaction->getStopIdTag()) {
        payload["idTag"] = (char*) transaction->getStopIdTag();
    }

    payload["meterStop"] = transaction->getMeterStop();

    char timestamp[JSONDATE_LENGTH + 1] = {'\0'};
    transaction->getStopTimestamp().toJsonString(timestamp, JSONDATE_LENGTH + 1);
    payload["timestamp"] = timestamp;

    payload["transactionId"] = transaction->getTransactionId();
    
    if (transaction->getStopReason() && *transaction->getStopReason()) {
        payload["reason"] = (char*) transaction->getStopReason();
    }

    if (!transactionData.empty()) {
        payload["transactionData"] = txDataDoc;
    }

    return doc;
}

void StopTransaction::processConf(JsonObject payload) {

    if (transaction) {
        transaction->getStopSync().confirm();
        transaction->commit();
    }

    MO_DBG_INFO("Request has been accepted!");

#if MO_ENABLE_LOCAL_AUTH
    if (auto authService = model.getAuthorizationService()) {
        authService->notifyAuthorization(transaction->getIdTag(), payload["idTagInfo"]);
    }
#endif //MO_ENABLE_LOCAL_AUTH
}

bool StopTransaction::processErr(const char *code, const char *description, JsonObject details) {

    if (transaction) {
        transaction->getStopSync().confirm(); //no retry behavior for now; consider data "arrived" at server
        transaction->commit();
    }

    MO_DBG_ERR("Server error, data loss!");

    return false;
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
