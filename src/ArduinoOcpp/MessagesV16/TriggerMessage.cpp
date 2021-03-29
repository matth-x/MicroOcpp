// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/MessagesV16/TriggerMessage.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <Variants.h>

using ArduinoOcpp::Ocpp16::TriggerMessage;

TriggerMessage::TriggerMessage() {
  statusMessage = "NotImplemented"; //default value if anything goes wrong
}

const char* TriggerMessage::getOcppOperationType(){
    return "TriggerMessage";
}

void TriggerMessage::processReq(JsonObject payload) {

  Serial.print(F("[TriggerMessage] Warning: TriggerMessage is not tested!\n"));

  triggeredOperation = makeFromTriggerMessage(payload);
  if (triggeredOperation != NULL) {
    statusMessage = "Accepted";
  } else {
    Serial.print(F("[TriggerMessage] Couldn't make OppOperation from TriggerMessage. Ignore request.\n"));
    statusMessage = "NotImplemented";
  }
}

DynamicJsonDocument* TriggerMessage::createConf(){
  DynamicJsonDocument* doc = new DynamicJsonDocument(JSON_OBJECT_SIZE(1) + strlen(statusMessage));
  JsonObject payload = doc->to<JsonObject>();
  
  payload["status"] = statusMessage;

  initiateOcppOperation(triggeredOperation);

  return doc;
}
