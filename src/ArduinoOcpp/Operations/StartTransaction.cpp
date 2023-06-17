// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Operations/StartTransaction.h>
#include <ArduinoOcpp/Model/Model.h>
#include <ArduinoOcpp/Core/RequestStore.h>
#include <ArduinoOcpp/Model/Authorization/AuthorizationService.h>
#include <ArduinoOcpp/Model/Metering/MeteringService.h>
#include <ArduinoOcpp/Model/Transactions/TransactionStore.h>
#include <ArduinoOcpp/Model/Transactions/Transaction.h>
#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::Ocpp16::StartTransaction;


StartTransaction::StartTransaction(Model& model, std::shared_ptr<Transaction> transaction) : model(model), transaction(transaction) {
    
}

StartTransaction::~StartTransaction() {
    
}

const char* StartTransaction::getOperationType() {
    return "StartTransaction";
}

void StartTransaction::initiate(StoredOperationHandler *opStore) {
    if (!transaction || transaction->getStartSync().isRequested()) {
        AO_DBG_ERR("initialization error");
        return;
    }
    
    auto payload = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(2)));
    (*payload)["connectorId"] = transaction->getConnectorId();
    (*payload)["txNr"] = transaction->getTxNr();

    if (opStore) {
        opStore->setPayload(std::move(payload));
        opStore->commit();
    }

    transaction->getStartSync().setRequested();

    transaction->commit();

    AO_DBG_INFO("StartTransaction initiated");
}

bool StartTransaction::restore(StoredOperationHandler *opStore) {
    if (!opStore) {
        AO_DBG_ERR("invalid argument");
        return false;
    }

    auto payload = opStore->getPayload();
    if (!payload) {
        AO_DBG_ERR("memory corruption");
        return false;
    }

    int connectorId = (*payload)["connectorId"] | -1;
    int txNr = (*payload)["txNr"] | -1;
    if (connectorId < 0 || txNr < 0) {
        AO_DBG_ERR("record incomplete");
        return false;
    }

    auto txStore = model.getTransactionStore();

    if (!txStore) {
        AO_DBG_ERR("invalid state");
        return false;
    }

    transaction = txStore->getTransaction(connectorId, txNr);
    if (!transaction) {
        AO_DBG_ERR("referential integrity violation");

        //clean up possible tx records
        if (auto mSerivce = model.getMeteringService()) {
            mSerivce->removeTxMeterData(connectorId, txNr);
        }
        return false;
    }

    return true;
}

std::unique_ptr<DynamicJsonDocument> StartTransaction::createReq() {

    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
                JSON_OBJECT_SIZE(6) + 
                (IDTAG_LEN_MAX + 1) +
                (JSONDATE_LENGTH + 1)));
                
    JsonObject payload = doc->to<JsonObject>();

    payload["connectorId"] = transaction->getConnectorId();

    if (transaction->getIdTag() && *transaction->getIdTag()) {
        payload["idTag"] = (char*) transaction->getIdTag();
    }

    if (transaction->isMeterStartDefined()) {
        payload["meterStart"] = transaction->getMeterStart();
    }

    if (transaction->getReservationId() >= 0) {
        payload["reservationId"] = transaction->getReservationId();
    }

    if (transaction->getStartTimestamp() > MIN_TIME) {
        char timestamp[JSONDATE_LENGTH + 1] = {'\0'};
        transaction->getStartTimestamp().toJsonString(timestamp, JSONDATE_LENGTH + 1);
        payload["timestamp"] = timestamp;
    }

    return doc;
}

void StartTransaction::processConf(JsonObject payload) {

    const char* idTagInfoStatus = payload["idTagInfo"]["status"] | "not specified";
    if (!strcmp(idTagInfoStatus, "Accepted")) {
        AO_DBG_INFO("Request has been accepted");
    } else {
        AO_DBG_INFO("Request has been denied. Reason: %s", idTagInfoStatus);
        transaction->setIdTagDeauthorized();
    }

    int transactionId = payload["transactionId"] | -1;
    transaction->setTransactionId(transactionId);

    transaction->getStartSync().confirm();
    transaction->commit();

    if (auto authService = model.getAuthorizationService()) {
        authService->notifyAuthorization(transaction->getIdTag(), payload["idTagInfo"]);
    }
}

void StartTransaction::processReq(JsonObject payload) {

  /**
   * Ignore Contents of this Req-message, because this is for debug purposes only
   */

}

std::unique_ptr<DynamicJsonDocument> StartTransaction::createConf() {
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2)));
    JsonObject payload = doc->to<JsonObject>();

    JsonObject idTagInfo = payload.createNestedObject("idTagInfo");
    idTagInfo["status"] = "Accepted";
    static int uniqueTxId = 1000;
    payload["transactionId"] = uniqueTxId++; //sample data for debug purpose

    return doc;
}
