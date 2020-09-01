// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#include "Variants.h"

#include "Reset.h"
#include "OcppEngine.h"

Reset::Reset() {
  
}

const char* Reset::getOcppOperationType(){
    return "Reset";
}

void Reset::processReq(JsonObject payload) {
  /*
   * Process the application data here. Note: you have to implement the device reset procedure in your client code. You have to set
   * a onSendConfListener in which you initiate a reset (e.g. calling ESP.reset() )
   */
  const char *type = payload["type"] | "Invalid";
  if (!strcmp(type, "Hard")){
      Serial.print(F("[Reset] Warning: received request to perform hard reset, but this implementation is only capable of soft reset!\n"));
  }
}

DynamicJsonDocument* Reset::createConf(){
  DynamicJsonDocument* doc = new DynamicJsonDocument(JSON_OBJECT_SIZE(1));
  JsonObject payload = doc->to<JsonObject>();
  payload["status"] = "Accepted";
  return doc;
}
