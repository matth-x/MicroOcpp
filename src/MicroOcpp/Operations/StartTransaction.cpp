// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/StartTransaction.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Authorization/AuthorizationService.h>
#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Debug.h>
#include <MicroOcpp/Version.h>

using MicroOcpp::Ocpp16::StartTransaction;
using MicroOcpp::JsonDoc;


StartTransaction::StartTransaction(Model& model, std::shared_ptr<Transaction> transaction) : MemoryManaged("v16.Operation.", "StartTransaction"), model(model), transaction(transaction) {
    
}

StartTransaction::~StartTransaction() {
    
}

const char* StartTransaction::getOperationType() {
    return "StartTransaction";
}

std::unique_ptr<JsonDoc> StartTransaction::createReq() {

    auto doc = makeJsonDoc(getMemoryTag(),
                JSON_OBJECT_SIZE(6) + 
                (IDTAG_LEN_MAX + 1) +
                (JSONDATE_LENGTH + 1));
                
    JsonObject payload = doc->to<JsonObject>();

    payload["connectorId"] = transaction->getConnectorId();
    payload["idTag"] = (char*) transaction->getIdTag();
    payload["meterStart"] = transaction->getMeterStart();

    if (transaction->getReservationId() >= 0) {
        payload["reservationId"] = transaction->getReservationId();
    }

    if (transaction->getStartTimestamp() < MIN_TIME &&
            transaction->getStartBootNr() == model.getBootNr()) {
        MO_DBG_DEBUG("adjust preboot StartTx timestamp");
        Timestamp adjusted = model.getClock().adjustPrebootTimestamp(transaction->getStartTimestamp());
        transaction->setStartTimestamp(adjusted);
    }

    char timestamp[JSONDATE_LENGTH + 1] = {'\0'};
    transaction->getStartTimestamp().toJsonString(timestamp, JSONDATE_LENGTH + 1);
    payload["timestamp"] = timestamp;

    return doc;
}

void StartTransaction::processConf(JsonObject payload) {

    const char* idTagInfoStatus = payload["idTagInfo"]["status"] | "not specified";
    if (!strcmp(idTagInfoStatus, "Accepted")) {
        MO_DBG_INFO("Request has been accepted");
    } else {
        MO_DBG_INFO("Request has been denied. Reason: %s", idTagInfoStatus);
        transaction->setIdTagDeauthorized();
    }

    int transactionId = payload["transactionId"] | -1;
    transaction->setTransactionId(transactionId);

    if (payload["idTagInfo"].containsKey("parentIdTag"))
    {
        transaction->setParentIdTag(payload["idTagInfo"]["parentIdTag"] | "");
    }

    transaction->getStartSync().confirm();
    transaction->commit();

#if MO_ENABLE_LOCAL_AUTH
    if (auto authService = model.getAuthorizationService()) {
        authService->notifyAuthorization(transaction->getIdTag(), payload["idTagInfo"]);
    }
#endif //MO_ENABLE_LOCAL_AUTH
}

void StartTransaction::processReq(JsonObject payload) {

  /**
   * Ignore Contents of this Req-message, because this is for debug purposes only
   */

}

std::unique_ptr<JsonDoc> StartTransaction::createConf() {
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2));
    JsonObject payload = doc->to<JsonObject>();

    JsonObject idTagInfo = payload.createNestedObject("idTagInfo");
    idTagInfo["status"] = "Accepted";
    static int uniqueTxId = 1000;
    payload["transactionId"] = uniqueTxId++; //sample data for debug purpose

    return doc;
}
