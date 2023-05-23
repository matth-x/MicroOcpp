// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Core/Operation.h>

#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::Operation;

Operation::Operation() {}

Operation::~Operation() {}
  
const char* Operation::getOperationType(){
    AO_DBG_ERR("Unsupported operation: getOperationType() is not implemented");
    return "CustomOperation";
}

void Operation::initiate(StoredOperationHandler *rpcData) {
    //called after initiateRequest(anyMsg)
}

bool Operation::restore(StoredOperationHandler *rpcData) {
    return false;
}

std::unique_ptr<DynamicJsonDocument> Operation::createReq() {
    AO_DBG_ERR("Unsupported operation: createReq() is not implemented");
    return nullptr;
}

void Operation::processConf(JsonObject payload) {
    AO_DBG_ERR("Unsupported operation: processConf() is not implemented");
}

void Operation::processReq(JsonObject payload) {
    AO_DBG_ERR("Unsupported operation: processReq() is not implemented");
}

std::unique_ptr<DynamicJsonDocument> Operation::createConf() {
    AO_DBG_ERR("Unsupported operation: createConf() is not implemented");
    return nullptr;
}

std::unique_ptr<DynamicJsonDocument> ArduinoOcpp::createEmptyDocument() {
    auto emptyDoc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(0));
    emptyDoc->to<JsonObject>();
    return emptyDoc;
}
