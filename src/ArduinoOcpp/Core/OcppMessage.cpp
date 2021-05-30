// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/Core/OcppMessage.h>

using ArduinoOcpp::OcppMessage;

OcppMessage::OcppMessage(){}

OcppMessage::~OcppMessage(){}
  
const char* OcppMessage::getOcppOperationType(){
    Serial.print(F("[OcppMessage]  Unsupported operation: getOcppOperationType() is not implemented!\n"));
    return "CustomOperation";
}

void OcppMessage::initiate() {
    //callback after initiateOcppOperation(anyMsg)
}

DynamicJsonDocument* OcppMessage::createReq() {
    Serial.print(F("[OcppMessage]  Unsupported operation: createReq() is not implemented!\n"));
    return new DynamicJsonDocument(0);
}

void OcppMessage::processConf(JsonObject payload) {
    Serial.print(F("[OcppMessage]  Unsupported operation: processConf() is not implemented!\n"));
}

void OcppMessage::processReq(JsonObject payload) {
    Serial.print(F("[OcppMessage]  Unsupported operation: processReq() is not implemented!\n"));
}

DynamicJsonDocument* OcppMessage::createConf() {
    Serial.print(F("[OcppMessage]  Unsupported operation: createConf() is not implemented!\n"));
    return NULL;
}

DynamicJsonDocument *ArduinoOcpp::createEmptyDocument() {
  DynamicJsonDocument *emptyDoc = new DynamicJsonDocument(0);
  emptyDoc->to<JsonObject>();
  return emptyDoc;
}
