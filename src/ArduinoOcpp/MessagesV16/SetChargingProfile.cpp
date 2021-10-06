// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include "Variants.h"

#include <ArduinoOcpp/MessagesV16/SetChargingProfile.h>

using ArduinoOcpp::Ocpp16::SetChargingProfile;

SetChargingProfile::SetChargingProfile(SmartChargingService *smartChargingService) 
  : smartChargingService(smartChargingService) {

}

SetChargingProfile::SetChargingProfile(DynamicJsonDocument *payloadToClient) 
  : payloadToClient(payloadToClient) {

}

SetChargingProfile::~SetChargingProfile() {
    if (payloadToClient != NULL)
        delete payloadToClient;
}

const char* SetChargingProfile::getOcppOperationType(){
    return "SetChargingProfile";
}

void SetChargingProfile::processReq(JsonObject payload) {

    //int connectorID = payload["connectorId"];

    JsonObject csChargingProfiles = payload["csChargingProfiles"];

    smartChargingService->updateChargingProfile(&csChargingProfiles);
}

DynamicJsonDocument* SetChargingProfile::createConf(){ //TODO review
    DynamicJsonDocument* doc = new DynamicJsonDocument(JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = "Accepted";
    return doc;
}

DynamicJsonDocument* SetChargingProfile::createReq() {
    if (payloadToClient != NULL) {
        DynamicJsonDocument *result = new DynamicJsonDocument(*payloadToClient);
        return result;
    }
    return NULL;
}

void SetChargingProfile::processConf(JsonObject payload) {
    const char* status = payload["status"] | "Invalid";
    if (strcmp(status, "Accepted")) {
        Serial.println(F("[SetChargingProfile] Send profile: rejected by client!"));
    }
}
