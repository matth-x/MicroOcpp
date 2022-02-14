// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Core/OcppMessage.h>

#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::OcppMessage;

OcppMessage::OcppMessage() {}

OcppMessage::~OcppMessage() {}
  
const char* OcppMessage::getOcppOperationType(){
    AO_DBG_ERR("Unsupported operation: getOcppOperationType() is not implemented");
    return "CustomOperation";
}

void OcppMessage::setOcppModel(std::shared_ptr<OcppModel> ocppModel) {
    if (!ocppModelInitialized) { //prevent the ocppModel from being overwritten
        this->ocppModel = ocppModel; //can still be nullptr
        ocppModelInitialized = true;
    }
}

void OcppMessage::initiate() {
    //called after initiateOcppOperation(anyMsg)
}

std::unique_ptr<DynamicJsonDocument> OcppMessage::createReq() {
    AO_DBG_ERR("Unsupported operation: createReq() is not implemented");
    return nullptr;
}

void OcppMessage::processConf(JsonObject payload) {
    AO_DBG_ERR("Unsupported operation: processConf() is not implemented");
}

void OcppMessage::processReq(JsonObject payload) {
    AO_DBG_ERR("Unsupported operation: processReq() is not implemented");
}

std::unique_ptr<DynamicJsonDocument> OcppMessage::createConf() {
    AO_DBG_ERR("Unsupported operation: createConf() is not implemented");
    return nullptr;
}

std::unique_ptr<DynamicJsonDocument> ArduinoOcpp::createEmptyDocument() {
    auto emptyDoc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(0));
    emptyDoc->to<JsonObject>();
    return std::move(emptyDoc);
}
