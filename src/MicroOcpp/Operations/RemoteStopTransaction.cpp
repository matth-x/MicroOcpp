// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/RemoteStopTransaction.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/ConnectorBase/Connector.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>

using MicroOcpp::Ocpp16::RemoteStopTransaction;

RemoteStopTransaction::RemoteStopTransaction(Model& model) : model(model) {
  
}

const char* RemoteStopTransaction::getOperationType(){
    return "RemoteStopTransaction";
}

void RemoteStopTransaction::processReq(JsonObject payload) {

    if (!payload.containsKey("transactionId")) {
        errorCode = "FormationViolation";
    }
    int transactionId = payload["transactionId"];

    for (unsigned int cId = 0; cId < model.getNumConnectors(); cId++) {
        auto connector = model.getConnector(cId);
        if (connector->getTransaction() &&
                connector->getTransaction()->getTransactionId() == transactionId) {
            connector->endTransaction(nullptr, "Remote");
            accepted = true;
        }
    }
}

std::unique_ptr<DynamicJsonDocument> RemoteStopTransaction::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    if (accepted){
        payload["status"] = "Accepted";
    } else {
        payload["status"] = "Rejected";
    }
    return doc;
}
