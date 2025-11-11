// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_LOCAL_AUTH

#include <MicroOcpp/Operations/GetLocalListVersion.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Authorization/AuthorizationService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::GetLocalListVersion;
using MicroOcpp::JsonDoc;

GetLocalListVersion::GetLocalListVersion(Model& model) : MemoryManaged("v16.Operation.", "GetLocalListVersion"), model(model) {
  
}

const char* GetLocalListVersion::getOperationType(){
    return "GetLocalListVersion";
}

void GetLocalListVersion::processReq(JsonObject payload) {
    //empty payload
}

std::unique_ptr<JsonDoc> GetLocalListVersion::createConf(){
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();

    auto authService = model.getAuthorizationService();
    if (authService && authService->localAuthListEnabled()) {
        payload["listVersion"] = authService->getLocalListVersion();
    } else {
        //TC_042_1_CS Get Local List Version (not supported)
        payload["listVersion"] = -1;
    }
    return doc;
}

#endif //MO_ENABLE_LOCAL_AUTH
