// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/MessagesV16/RemoteStopTransaction.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargeControl/Connector.h>

using ArduinoOcpp::Ocpp16::RemoteStopTransaction;

RemoteStopTransaction::RemoteStopTransaction(OcppModel& context) : context(context) {
  
}

const char* RemoteStopTransaction::getOcppOperationType(){
    return "RemoteStopTransaction";
}

void RemoteStopTransaction::processReq(JsonObject payload) {
    transactionId = payload["transactionId"] | -1;
}

std::unique_ptr<DynamicJsonDocument> RemoteStopTransaction::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    
    bool canStopTransaction = false;

    for (unsigned int cId = 0; cId < context.getNumConnectors(); cId++) {
        auto connector = context.getConnector(cId);
        if (connector->getTransactionId() == transactionId) {
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
