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
using MicroOcpp::MemJsonDoc;

GetLocalListVersion::GetLocalListVersion(Model& model) : AllocOverrider("v16.Operation.", getOperationType()), model(model) {
  
}

const char* GetLocalListVersion::getOperationType(){
    return "GetLocalListVersion";
}

void GetLocalListVersion::processReq(JsonObject payload) {
    //empty payload
}

std::unique_ptr<MemJsonDoc> GetLocalListVersion::createConf(){
    auto doc = makeMemJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();

    if (auto authService = model.getAuthorizationService()) {
        payload["listVersion"] = authService->getLocalListVersion();
    } else {
        payload["listVersion"] = -1;
    }
    return doc;
}

#endif //MO_ENABLE_LOCAL_AUTH
