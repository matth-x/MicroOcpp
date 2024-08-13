// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/RemoteStopTransaction.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/ConnectorBase/Connector.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>

using MicroOcpp::Ocpp16::RemoteStopTransaction;
using MicroOcpp::JsonDoc;

RemoteStopTransaction::RemoteStopTransaction(Model& model) : MemoryManaged("v16.Operation.", "RemoteStopTransaction"), model(model) {
  
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

std::unique_ptr<JsonDoc> RemoteStopTransaction::createConf(){
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    if (accepted){
        payload["status"] = "Accepted";
    } else {
        payload["status"] = "Rejected";
    }
    return doc;
}
