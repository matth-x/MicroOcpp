// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#include "Variants.h"

#include "RemoteStartTransaction.h"
#include "OcppEngine.h"

RemoteStartTransaction::RemoteStartTransaction() {
  
}

const char* RemoteStartTransaction::getOcppOperationType(){
    return "RemoteStartTransaction";
}

void RemoteStartTransaction::processReq(JsonObject payload) {
  idTag = String(payload["idTag"].as<String>());
}

DynamicJsonDocument* RemoteStartTransaction::createConf(){
  DynamicJsonDocument* doc = new DynamicJsonDocument(JSON_OBJECT_SIZE(1));
  JsonObject payload = doc->to<JsonObject>();
  payload["status"] = "Accepted";
  return doc;
}

DynamicJsonDocument* RemoteStartTransaction::createReq() {
  DynamicJsonDocument *doc = new DynamicJsonDocument(JSON_OBJECT_SIZE(1));
  JsonObject payload = doc->to<JsonObject>();

  payload["idTag"] = "fefed1d19876";

  return doc;
}

void RemoteStartTransaction::processConf(JsonObject payload){
    String status = payload["status"] | "Invalid";

    if (status.equals("Accepted")) {
        if (DEBUG_OUT) Serial.print(F("[RemoteStartTransaction] Request has been accepted!\n"));
    } else {
        Serial.print(F("[RemoteStartTransaction] Request has been denied!"));
    }
}
