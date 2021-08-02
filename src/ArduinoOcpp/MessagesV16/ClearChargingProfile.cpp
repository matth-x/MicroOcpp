// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/MessagesV16/ClearChargingProfile.h>
#include <ArduinoOcpp/Tasks/SmartCharging/SmartChargingService.h>
#include <ArduinoOcpp/Core/OcppEngine.h>

#include <functional>

using ArduinoOcpp::Ocpp16::ClearChargingProfile;

ClearChargingProfile::ClearChargingProfile() {

}

const char* ClearChargingProfile::getOcppOperationType(){
    return "ClearChargingProfile";
}

void ClearChargingProfile::processReq(JsonObject payload) {

    std::function<bool(int, int, ChargingProfilePurposeType, int)> filter = [payload = payload]
            (int chargingProfileId, int connectorId, ChargingProfilePurposeType chargingProfilePurpose, int stackLevel) {
        
        if (payload.containsKey("id")) {
            if (chargingProfileId != payload["id"] | -1) {
                return false;
            }
        }

        if (payload.containsKey("connectorId")) {
            Serial.print(F("[ClearChargingProfile] Smart Charging does not implement multiple connectors yet. Ignore connectorId\n"));
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
            if (stackLevel != payload["stackLevel"] | -1) {
                return false;
            }
        }

        return true;
    };

    SmartChargingService *smartChargingService = getSmartChargingService();
    if (!smartChargingService) {
        Serial.println(F("[ClearChargingProfile] SmartChargingService not initialized! Ignore request"));
        return;
    }

    matchingProfilesFound = smartChargingService->clearChargingProfile(filter);
}

DynamicJsonDocument* ClearChargingProfile::createConf(){
    DynamicJsonDocument* doc = new DynamicJsonDocument(JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    if (matchingProfilesFound)
        payload["status"] = "Accepted";
    else
        payload["status"] = "Unknown";
        
    return doc;
}
