// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/MessagesV16/SetChargingProfile.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/SmartCharging/SmartChargingService.h>
#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::Ocpp16::SetChargingProfile;

SetChargingProfile::SetChargingProfile() {

}

SetChargingProfile::SetChargingProfile(std::unique_ptr<DynamicJsonDocument> payloadToClient) 
  : payloadToClient{std::move(payloadToClient)} {

}

SetChargingProfile::~SetChargingProfile() {

}

const char* SetChargingProfile::getOcppOperationType(){
    return "SetChargingProfile";
}

void SetChargingProfile::processReq(JsonObject payload) {

    //int connectorID = payload["connectorId"];

    JsonObject csChargingProfiles = payload["csChargingProfiles"];

    if (ocppModel && ocppModel->getSmartChargingService()) {
        auto smartChargingService = ocppModel->getSmartChargingService();
        smartChargingService->updateChargingProfile(&csChargingProfiles);
    }
}

std::unique_ptr<DynamicJsonDocument> SetChargingProfile::createConf(){ //TODO review
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = "Accepted";
    return doc;
}

std::unique_ptr<DynamicJsonDocument> SetChargingProfile::createReq() {
    if (payloadToClient != nullptr) {
        auto result = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(*payloadToClient));
        return result;
    }
    return nullptr;
}

void SetChargingProfile::processConf(JsonObject payload) {
    const char* status = payload["status"] | "Invalid";
    if (strcmp(status, "Accepted")) {
        AO_DBG_WARN("Send profile: rejected by client");
    }
}
