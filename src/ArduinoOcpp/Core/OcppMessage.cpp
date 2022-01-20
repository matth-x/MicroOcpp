// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Core/OcppMessage.h>

#include <Variants.h>

using ArduinoOcpp::OcppMessage;

OcppMessage::OcppMessage() {}

OcppMessage::~OcppMessage() {}
  
const char* OcppMessage::getOcppOperationType(){
    Serial.print(F("[OcppMessage]  Unsupported operation: getOcppOperationType() is not implemented!\n"));
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
    Serial.print(F("[OcppMessage]  Unsupported operation: createReq() is not implemented!\n"));
    return nullptr;
}

void OcppMessage::processConf(JsonObject payload) {
    Serial.print(F("[OcppMessage]  Unsupported operation: processConf() is not implemented!\n"));
}

void OcppMessage::processReq(JsonObject payload) {
    Serial.print(F("[OcppMessage]  Unsupported operation: processReq() is not implemented!\n"));
}

std::unique_ptr<DynamicJsonDocument> OcppMessage::createConf() {
    Serial.print(F("[OcppMessage]  Unsupported operation: createConf() is not implemented!\n"));
    return nullptr;
}

std::unique_ptr<DynamicJsonDocument> ArduinoOcpp::createEmptyDocument() {
    auto emptyDoc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(0));
    emptyDoc->to<JsonObject>();
    return std::move(emptyDoc);
}
