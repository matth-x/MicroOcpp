// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include "Configuration.h"
#include <MicroOcpp/Version.h>

#if MO_ENABLE_LOCAL_AUTH

#include <MicroOcpp/Operations/GetLocalListVersion.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Authorization/AuthorizationService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::GetLocalListVersion;

GetLocalListVersion::GetLocalListVersion(Model& model) : model(model) {
  
}

const char* GetLocalListVersion::getOperationType(){
    return "GetLocalListVersion";
}

void GetLocalListVersion::processReq(JsonObject payload) {
    //empty payload
}

std::unique_ptr<DynamicJsonDocument> GetLocalListVersion::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    auto supported_feature_profiles = declareConfiguration<const char*>("SupportedFeatureProfiles", "");

    if (strstr(supported_feature_profiles->getString(), "LocalAuthListManagement") == NULL) {
        payload["listVersion"] = -1;
    } else if (auto authService = model.getAuthorizationService()) {
        payload["listVersion"] = authService->getLocalListVersion();
    } else {
        payload["listVersion"] = -1;
    }
    return doc;
}

#endif //MO_ENABLE_LOCAL_AUTH
