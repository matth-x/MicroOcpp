// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/Authorize.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Authorization/AuthorizationService.h>

#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

#if MO_ENABLE_V16

Ocpp16::Authorize::Authorize(Model& model, const char *idTagIn) : MemoryManaged("v16.Operation.", "Authorize"), model(model) {
    if (idTagIn && strnlen(idTagIn, IDTAG_LEN_MAX + 2) <= IDTAG_LEN_MAX) {
        snprintf(idTag, IDTAG_LEN_MAX + 1, "%s", idTagIn);
    } else {
        MO_DBG_WARN("Format violation of idTag. Discard idTag");
    }
}

const char* Ocpp16::Authorize::getOperationType(){
    return "Authorize";
}

std::unique_ptr<JsonDoc> Ocpp16::Authorize::createReq() {
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1) + (IDTAG_LEN_MAX + 1));
    JsonObject payload = doc->to<JsonObject>();
    payload["idTag"] = idTag;
    return doc;
}

void Ocpp16::Authorize::processConf(JsonObject payload){
    const char *idTagInfo = payload["idTagInfo"]["status"] | "not specified";

    if (!strcmp(idTagInfo, "Accepted")) {
        MO_DBG_INFO("Request has been accepted");
    } else {
        MO_DBG_INFO("Request has been denied. Reason: %s", idTagInfo);
    }

#if MO_ENABLE_LOCAL_AUTH
    if (auto authService = model.getAuthorizationService()) {
        authService->notifyAuthorization(idTag, payload["idTagInfo"]);
    }
#endif //MO_ENABLE_LOCAL_AUTH
}

#if MO_ENABLE_MOCK_SERVER
int Ocpp16::Authorize::writeMockConf(const char *operationType, char *buf, size_t size, int userStatus, void *userData) {
    (void)userStatus;
    (void)userData;
    return snprintf(buf, size, "{\"idTagInfo\":{\"status\":\"Accepted\"}}");
}
#endif //MO_ENABLE_MOCK_SERVER

#endif //MO_ENABLE_V16

#if MO_ENABLE_V201

Ocpp201::Authorize::Authorize(Model& model, const IdToken& idToken) : MemoryManaged("v201.Operation.Authorize"), model(model) {
    this->idToken = idToken;
}

const char* Ocpp201::Authorize::getOperationType(){
    return "Authorize";
}

std::unique_ptr<JsonDoc> Ocpp201::Authorize::createReq() {
    auto doc = makeJsonDoc(getMemoryTag(),
            JSON_OBJECT_SIZE(1) +
            JSON_OBJECT_SIZE(2));
    JsonObject payload = doc->to<JsonObject>();
    payload["idToken"]["idToken"] = idToken.get();
    payload["idToken"]["type"] = idToken.getTypeCstr();
    return doc;
}

void Ocpp201::Authorize::processConf(JsonObject payload){
    const char *idTagInfo = payload["idTokenInfo"]["status"] | "_Undefined";

    if (!strcmp(idTagInfo, "Accepted")) {
        MO_DBG_INFO("Request has been accepted");
    } else {
        MO_DBG_INFO("Request has been denied. Reason: %s", idTagInfo);
    }

    //if (model.getAuthorizationService()) {
    //    model.getAuthorizationService()->notifyAuthorization(idTag, payload["idTagInfo"]);
    //}
}

#if MO_ENABLE_MOCK_SERVER
int Ocpp201::Authorize::writeMockConf(const char *operationType, char *buf, size_t size, int userStatus, void *userData) {
    (void)userStatus;
    (void)userData;
    return snprintf(buf, size, "{\"idTokenInfo\":{\"status\":\"Accepted\"}}");
}
#endif //MO_ENABLE_MOCK_SERVER

#endif //MO_ENABLE_V201
