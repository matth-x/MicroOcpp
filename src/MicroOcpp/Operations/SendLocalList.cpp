// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Operations/SendLocalList.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Authorization/AuthorizationService.h>

using MicroOcpp::Ocpp16::SendLocalList;

SendLocalList::SendLocalList(Model& model) : model(model) {
  
}

SendLocalList::~SendLocalList() {
  
}

const char* SendLocalList::getOperationType(){
    return "SendLocalList";
}

void SendLocalList::processReq(JsonObject payload) {
    if (!payload.containsKey("listVersion") || !payload.containsKey("updateType")) {
        errorCode = "FormationViolation";
        return;
    }

    auto authService = model.getAuthorizationService();

    if (!authService) {
        errorCode = "InternalError";
        return;
    }

    if (payload["localAuthorizationList"].as<JsonArray>().size() > MOCPP_SendLocalListMaxLength) {
        errorCode = "OccurenceConstraintViolation";
    }

    int listVersion = payload["listVersion"];

    if (authService->getLocalListVersion() >= listVersion) {
        versionMismatch = true;
        return;
    }

    updateFailure = !authService->updateLocalList(payload);
}

std::unique_ptr<DynamicJsonDocument> SendLocalList::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();

    if (versionMismatch) {
        payload["status"] = "VersionMismatch";
    } else if (updateFailure) {
        payload["status"] = "Failed";
    } else {
        payload["status"] = "Accepted";
    }
    
    return doc;
}
