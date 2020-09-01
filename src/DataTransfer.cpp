// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#include "DataTransfer.h"
#include "OcppEngine.h"
#include "Variants.h"

DataTransfer::DataTransfer(String &msg) {
    this->msg = String(msg);
}

const char* DataTransfer::getOcppOperationType(){
    return "DataTransfer";
}

DynamicJsonDocument* DataTransfer::createReq() {
  DynamicJsonDocument *doc = new DynamicJsonDocument(JSON_OBJECT_SIZE(2) + (msg.length() + 1));
  JsonObject payload = doc->to<JsonObject>();
  payload["vendorId"] = "CustomVendor";
  payload["data"] = msg;
  return doc;
}

void DataTransfer::processConf(JsonObject payload){
    String status = payload["status"] | "Invalid";

    if (status.equals("Accepted")) {
        if (DEBUG_OUT) Serial.print(F("[DataTransfer] Request has been accepted!\n"));
    } else {
        Serial.print(F("[DataTransfer] Request has been denied!"));
    }
}
