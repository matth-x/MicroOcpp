// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_LOCAL_AUTH

#include <MicroOcpp/Operations/SendLocalList.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Authorization/AuthorizationService.h>

using MicroOcpp::Ocpp16::SendLocalList;
using MicroOcpp::JsonDoc;

SendLocalList::SendLocalList(AuthorizationService& authService) : MemoryManaged("v16.Operation.", "SendLocalList"), authService(authService) {
  
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

    if (payload.containsKey("localAuthorizationList") && !payload["localAuthorizationList"].is<JsonArray>()) {
        errorCode = "FormationViolation";
        return;
    }

    JsonArray localAuthorizationList = payload["localAuthorizationList"];

    if (localAuthorizationList.size() > MO_SendLocalListMaxLength) {
        errorCode = "OccurenceConstraintViolation";
        return;
    }

    bool differential = !strcmp("Differential", payload["updateType"] | "Invalid"); //updateType Differential or Full

    int listVersion = payload["listVersion"];

    if (differential && authService.getLocalListVersion() >= listVersion) {
        versionMismatch = true;
        return;
    }

    updateFailure = !authService.updateLocalList(localAuthorizationList, listVersion, differential);
}

std::unique_ptr<JsonDoc> SendLocalList::createConf(){
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
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

#endif //MO_ENABLE_LOCAL_AUTH
