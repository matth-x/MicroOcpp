// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Operations/StopTransaction.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/RequestStore.h>
#include <MicroOcpp/Model/Authorization/AuthorizationService.h>
#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Model/Metering/MeterValue.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Debug.h>

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

void StopTransaction::initiate(StoredOperationHandler *opStore) {
    if (!transaction || transaction->getStopSync().isRequested()) {
        MO_DBG_ERR("initialization error");
        return;
    }
    
    auto payload = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(2)));
    (*payload)["connectorId"] = transaction->getConnectorId();
    (*payload)["txNr"] = transaction->getTxNr();

    if (opStore) {
        opStore->setPayload(std::move(payload));
        opStore->commit();
    }

    transaction->getStopSync().setRequested();

    transaction->commit();

    MO_DBG_INFO("StopTransaction initiated");
}

bool StopTransaction::restore(StoredOperationHandler *opStore) {
    if (!opStore) {
        MO_DBG_ERR("invalid argument");
        return false;
    }

    auto payload = opStore->getPayload();
    if (!payload) {
        MO_DBG_ERR("memory corruption");
        return false;
    }

    int connectorId = (*payload)["connectorId"] | -1;
    int txNr = (*payload)["txNr"] | -1;
    if (connectorId < 0 || txNr < 0) {
        MO_DBG_ERR("record incomplete");
        return false;
    }

    auto txStore = model.getTransactionStore();

    if (!txStore) {
        MO_DBG_ERR("invalid state");
        return false;
    }

    transaction = txStore->getTransaction(connectorId, txNr);
    if (!transaction) {
        MO_DBG_ERR("referential integrity violation");

        //clean up possible tx records
        if (auto mSerivce = model.getMeteringService()) {
            mSerivce->removeTxMeterData(connectorId, txNr);
        }
        return false;
    }

    if (transaction->isSilent()) {
        //transaction has been set silent after initializing StopTx - discard operation record
        MO_DBG_WARN("tx has been set silent - discard StopTx");

        //clean up possible tx records
        if (auto mSerivce = model.getMeteringService()) {
            mSerivce->removeTxMeterData(connectorId, txNr);
        }
        return false;
    }

    if (auto mSerivce = model.getMeteringService()) {
        if (auto txData = mSerivce->getStopTxMeterData(transaction.get())) {
            transactionData = txData->retrieveStopTxData();
        }
    }

    return true;
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
            (void)0; //send invalid value
        }
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

    if (auto authService = model.getAuthorizationService()) {
        authService->notifyAuthorization(transaction->getIdTag(), payload["idTagInfo"]);
    }
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
