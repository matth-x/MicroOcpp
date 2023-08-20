// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Operations/ClearChargingProfile.h>
#include <MicroOcpp/Model/SmartCharging/SmartChargingService.h>
#include <MicroOcpp/Debug.h>

#include <functional>

using MicroOcpp::Ocpp16::ClearChargingProfile;

ClearChargingProfile::ClearChargingProfile(SmartChargingService& scService) : scService(scService) {

}

const char* ClearChargingProfile::getOperationType(){
    return "ClearChargingProfile";
}

void ClearChargingProfile::processReq(JsonObject payload) {

    std::function<bool(int, int, ChargingProfilePurposeType, int)> filter = [payload]
            (int chargingProfileId, int connectorId, ChargingProfilePurposeType chargingProfilePurpose, int stackLevel) {
        
        if (payload.containsKey("id")) {
            if (chargingProfileId == (payload["id"] | -1)) {
                return true;
            } else {
                return false;
            }
        }

        if (payload.containsKey("connectorId")) {
            if (connectorId != (payload["connectorId"] | -1)) {
                return false;
            }
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

    matchingProfilesFound = scService.clearChargingProfile(filter);
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
