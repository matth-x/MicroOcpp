// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Core/Operation.h>

#include <MicroOcpp/Debug.h>

using MicroOcpp::Operation;

Operation::Operation() {}

Operation::~Operation() {}
  
const char* Operation::getOperationType(){
    MO_DBG_ERR("Unsupported operation: getOperationType() is not implemented");
    return "CustomOperation";
}

void Operation::initiate(StoredOperationHandler *rpcData) {
    //called after initiateRequest(anyMsg)
}

bool Operation::restore(StoredOperationHandler *rpcData) {
    return false;
}

std::unique_ptr<DynamicJsonDocument> Operation::createReq() {
    MO_DBG_ERR("Unsupported operation: createReq() is not implemented");
    return createEmptyDocument();
}

void Operation::processConf(JsonObject payload) {
    MO_DBG_ERR("Unsupported operation: processConf() is not implemented");
}

void Operation::processReq(JsonObject payload) {
    MO_DBG_ERR("Unsupported operation: processReq() is not implemented");
}

std::unique_ptr<DynamicJsonDocument> Operation::createConf() {
    MO_DBG_ERR("Unsupported operation: createConf() is not implemented");
    return createEmptyDocument();
}

std::unique_ptr<DynamicJsonDocument> MicroOcpp::createEmptyDocument() {
    auto emptyDoc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(0));
    emptyDoc->to<JsonObject>();
    return emptyDoc;
}
