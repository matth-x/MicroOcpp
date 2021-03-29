// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/MessagesV16/Heartbeat.h>
#include <ArduinoOcpp/TimeHelper.h>
#include <string.h>

using ArduinoOcpp::Ocpp16::Heartbeat;

Heartbeat::Heartbeat()  {
  
}

const char* Heartbeat::getOcppOperationType(){
    return "Heartbeat";
}

DynamicJsonDocument* Heartbeat::createReq() {
  DynamicJsonDocument *doc = new DynamicJsonDocument(0);
  JsonObject payload = doc->to<JsonObject>();
  /*
   * Empty payload
   */
  return doc;
}

void Heartbeat::processConf(JsonObject payload) {
  
  const char* currentTime = payload["currentTime"] | "Invalid";
  if (strcmp(currentTime, "Invalid")) {
    if (setTimeFromJsonDateString(currentTime)) {
      if (DEBUG_OUT) Serial.print(F("[Heartbeat] Request has been accepted!\n"));
    } else {
      Serial.print(F("[Heartbeat] Request accepted. But Error reading time string. Expect format like 2020-02-01T20:53:32.486Z\n"));
    }
  } else {
    Serial.print(F("[Heartbeat] Request denied. Missing field currentTime. Expect format like 2020-02-01T20:53:32.486Z\n"));
  }

}

void Heartbeat::processReq(JsonObject payload) {

  /**
   * Ignore Contents of this Req-message, because this is for debug purposes only
   */

}

DynamicJsonDocument* Heartbeat::createConf(){
  DynamicJsonDocument* doc = new DynamicJsonDocument(JSON_OBJECT_SIZE(1) + (JSONDATE_LENGTH + 1));
  JsonObject payload = doc->to<JsonObject>();

  char currentTime[JSONDATE_LENGTH + 1] = {'\0'};
  getJsonDateStringFromSystemTime(currentTime, JSONDATE_LENGTH);
  payload["currentTime"] = currentTime;

  return doc;
}
