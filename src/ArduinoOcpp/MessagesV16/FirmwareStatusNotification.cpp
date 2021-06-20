// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/MessagesV16/FirmwareStatusNotification.h>

using ArduinoOcpp::Ocpp16::FirmwareStatusNotification;

FirmwareStatusNotification::FirmwareStatusNotification() {
    status = String("Idle"); // TriggerMessage will use this constructor. Should replace with actual value later
}

FirmwareStatusNotification::FirmwareStatusNotification(String &status) {
    this->status = String(status);
}

FirmwareStatusNotification::FirmwareStatusNotification(const char *status) {
    this->status = status;
}

DynamicJsonDocument* FirmwareStatusNotification::createReq() {
  DynamicJsonDocument *doc = new DynamicJsonDocument(JSON_OBJECT_SIZE(1) + (status.length() + 1));
  JsonObject payload = doc->to<JsonObject>();
  payload["status"] = status;
  return doc;
}

void FirmwareStatusNotification::processConf(JsonObject payload){
    // no payload, nothing to do
}
