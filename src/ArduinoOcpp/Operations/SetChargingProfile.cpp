// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Operations/SetChargingProfile.h>
#include <ArduinoOcpp/Model/Model.h>
#include <ArduinoOcpp/Model/SmartCharging/SmartChargingService.h>
#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::Ocpp16::SetChargingProfile;

SetChargingProfile::SetChargingProfile(Model& model) : model(model) {

}

SetChargingProfile::SetChargingProfile(Model& model, std::unique_ptr<DynamicJsonDocument> payloadToClient)
        : model(model), payloadToClient{std::move(payloadToClient)} {
}

SetChargingProfile::~SetChargingProfile() {

}

const char* SetChargingProfile::getOperationType(){
    return "SetChargingProfile";
}

void SetChargingProfile::processReq(JsonObject payload) {

    //int connectorID = payload["connectorId"];

    JsonObject csChargingProfiles = payload["csChargingProfiles"];

    if (auto scService = model.getSmartChargingService()) {
        scService->setChargingProfile(csChargingProfiles);
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
