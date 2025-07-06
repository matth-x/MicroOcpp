// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/GetLocalListVersion.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Authorization/AuthorizationService.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16 && MO_ENABLE_LOCAL_AUTH

using namespace MicroOcpp;
using namespace MicroOcpp::v16;

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

    if (auto authService = model.getAuthorizationService()) {
        payload["listVersion"] = authService->getLocalListVersion();
    } else {
        payload["listVersion"] = -1;
    }
    return doc;
}

#endif //MO_ENABLE_V16 && MO_ENABLE_LOCAL_AUTH
