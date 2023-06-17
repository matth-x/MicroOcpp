// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Operations/ClearChargingProfile.h>
#include <ArduinoOcpp/Model/Model.h>
#include <ArduinoOcpp/Model/SmartCharging/SmartChargingService.h>
#include <ArduinoOcpp/Debug.h>

#include <functional>

using ArduinoOcpp::Ocpp16::ClearChargingProfile;

ClearChargingProfile::ClearChargingProfile(Model& model) : model(model) {

}

const char* ClearChargingProfile::getOperationType(){
    return "ClearChargingProfile";
}

void ClearChargingProfile::processReq(JsonObject payload) {

    std::function<bool(int, int, ChargingProfilePurposeType, int)> filter = [payload]
            (int chargingProfileId, int connectorId, ChargingProfilePurposeType chargingProfilePurpose, int stackLevel) {
        
        if (payload.containsKey("id")) {
            if (chargingProfileId != (payload["id"] | -1)) {
                return false;
            }
        }

        if (payload.containsKey("connectorId")) {
            AO_DBG_WARN("Smart Charging does not implement multiple connectors yet. Ignore connectorId");
        }

        if (payload.containsKey("chargingProfilePurpose")) {
            switch (chargingProfilePurpose) {
                case ChargingProfilePurposeType::ChargePointMaxProfile:
                    if (strcmp(payload["chargingProfilePurpose"] | "INVALID", "ChargePointMaxProfile")) {
                        return false;
                    }
                    break;
                case ChargingProfilePurposeType::TxDefaultProfile:
                    if (strcmp(payload["chargingProfilePurpose"] | "INVALID", "TxDefaultProfile")) {
                        return false;
                    }
                    break;
                case ChargingProfilePurposeType::TxProfile:
                    if (strcmp(payload["chargingProfilePurpose"] | "INVALID", "TxProfile")) {
                        return false;
                    }
                    break;
            }
        }

        if (payload.containsKey("stackLevel")) {
            if (stackLevel != (payload["stackLevel"] | -1)) {
                return false;
            }
        }

        return true;
    };

    if (auto scService = model.getSmartChargingService()) {
        matchingProfilesFound = scService->clearChargingProfile(filter);
    } else {
        AO_DBG_ERR("SmartChargingService not initialized");
        errorCode = "NotSupported";
    }
}

std::unique_ptr<DynamicJsonDocument> ClearChargingProfile::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    if (matchingProfilesFound)
        payload["status"] = "Accepted";
    else
        payload["status"] = "Unknown";
        
    return doc;
}
