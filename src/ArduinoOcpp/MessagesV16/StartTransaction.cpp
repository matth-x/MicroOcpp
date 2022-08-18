// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/MessagesV16/StartTransaction.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Tasks/Metering/MeteringService.h>
#include <ArduinoOcpp/Tasks/Transactions/TransactionService.h>
#include <ArduinoOcpp/Tasks/Transactions/Transaction.h>
#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::Ocpp16::StartTransaction;
using ArduinoOcpp::TransactionRPC;


StartTransaction::StartTransaction(std::shared_ptr<Transaction> transaction) : transaction(transaction) {
    
}

const char* StartTransaction::getOcppOperationType() {
    return "StartTransaction";
}

void StartTransaction::initiate() {
    if (ocppModel && transaction && !transaction->getStartRpcSync().isRequested()) {
        //fill out tx data if not happened before

        auto meteringService = ocppModel->getMeteringService();
        if (transaction->getMeterStart() < 0 && meteringService) {
            auto meterStart = meteringService->readTxEnergyMeter(transaction->getConnectorId(), ReadingContext::TransactionBegin);
            if (meterStart && *meterStart) {
                transaction->setMeterStart(meterStart->toInteger());
            } else {
                AO_DBG_ERR("MeterStart undefined");
            }
        }

        if (transaction->getStartTimestamp() <= MIN_TIME) {
            transaction->setStartTimestamp(ocppModel->getOcppTime().getOcppTimestampNow());
        }
        
        auto seqNr = ocppModel->getTransactionService()->getTransactionSequence().reserveSeqNr();
        AO_DBG_DEBUG("Reserved seqNr inside StartTx: %u", seqNr);
        transaction->getStartRpcSync().setRequested(seqNr);

        transaction->commit();
    }

    AO_DBG_INFO("StartTransaction initiated");
}

std::unique_ptr<DynamicJsonDocument> StartTransaction::createReq() {

    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
                JSON_OBJECT_SIZE(5) + 
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

    transaction->getStartRpcSync().confirm();
    transaction->commit();
}

TransactionRPC *StartTransaction::getTransactionSync() {
    return transaction ? &transaction->getStartRpcSync() : nullptr;
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
