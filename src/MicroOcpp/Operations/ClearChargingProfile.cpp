// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#include <MicroOcpp/Operations/ClearChargingProfile.h>
#include <MicroOcpp/Model/SmartCharging/SmartChargingService.h>
#include <MicroOcpp/Debug.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SMARTCHARGING

using namespace MicroOcpp;

ClearChargingProfile::ClearChargingProfile(SmartChargingService& scService, int ocppVersion) : MemoryManaged("v16.Operation.", "ClearChargingProfile"), scService(scService), ocppVersion(ocppVersion) {

}

const char* ClearChargingProfile::getOperationType(){
    return "ClearChargingProfile";
}

void ClearChargingProfile::processReq(JsonObject payload) {

    int chargingProfileId = -1;
    int evseId = -1;
    ChargingProfilePurposeType chargingProfilePurpose = ChargingProfilePurposeType::UNDEFINED;
    int stackLevel = -1;

    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {
        chargingProfileId = payload["id"] | -1;
        evseId = payload["connectorId"] | -1;
        chargingProfilePurpose = deserializeChargingProfilePurposeType(payload["chargingProfilePurpose"] | "_Undefined");
        stackLevel = payload["stackLevel"] | -1;
    }
    #endif //MO_ENABLE_V16
    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {
        if (payload.containsKey("chargingProfileId")) {
            chargingProfileId = payload["chargingProfileId"] | -1;
        } else {
            JsonObject chargingProfileCriteria = payload["chargingProfileCriteria"];
            evseId = chargingProfileCriteria["evseId"] | -1;
            chargingProfilePurpose = deserializeChargingProfilePurposeType(chargingProfileCriteria["chargingProfilePurpose"] | "_Undefined");
            stackLevel = chargingProfileCriteria["stackLevel"] | -1;
        }
    }
    #endif //MO_ENABLE_V201

    bool matchingProfilesFound = scService.clearChargingProfile(chargingProfileId, evseId, chargingProfilePurpose, stackLevel);
    if (matchingProfilesFound) {
        status = "Accepted";
    } else {
        status = "Unknown";
    }
}

std::unique_ptr<JsonDoc> ClearChargingProfile::createConf(){
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = status;
    return doc;
}

#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SMARTCHARGING
