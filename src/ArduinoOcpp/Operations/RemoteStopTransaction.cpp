// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Operations/RemoteStopTransaction.h>
#include <ArduinoOcpp/Model/Model.h>
#include <ArduinoOcpp/Model/ChargeControl/Connector.h>
#include <ArduinoOcpp/Model/Transactions/Transaction.h>

using ArduinoOcpp::Ocpp16::RemoteStopTransaction;

RemoteStopTransaction::RemoteStopTransaction(Model& model) : model(model) {
  
}

const char* RemoteStopTransaction::getOperationType(){
    return "RemoteStopTransaction";
}

void RemoteStopTransaction::processReq(JsonObject payload) {
    transactionId = payload["transactionId"] | -1;
}

std::unique_ptr<DynamicJsonDocument> RemoteStopTransaction::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    
    bool canStopTransaction = false;

    for (unsigned int cId = 0; cId < model.getNumConnectors(); cId++) {
        auto connector = model.getConnector(cId);
        if (connector->getTransaction() &&
                connector->getTransaction()->getTransactionId() == transactionId) {
            canStopTransaction = true;
            connector->endTransaction("Remote");
        }
    }

    if (canStopTransaction){
        payload["status"] = "Accepted";
    } else {
        payload["status"] = "Rejected";
    }
    
    return doc;
}
