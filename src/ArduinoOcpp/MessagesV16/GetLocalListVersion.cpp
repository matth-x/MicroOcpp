// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/MessagesV16/GetLocalListVersion.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/Authorization/AuthorizationService.h>
#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::Ocpp16::GetLocalListVersion;

GetLocalListVersion::GetLocalListVersion() {
  
}

const char* GetLocalListVersion::getOcppOperationType(){
    return "GetLocalListVersion";
}

void GetLocalListVersion::processReq(JsonObject payload) {
    //empty payload
}

std::unique_ptr<DynamicJsonDocument> GetLocalListVersion::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    if (ocppModel && ocppModel->getAuthorizationService()) {
        payload["listVersion"] = ocppModel->getAuthorizationService()->getLocalListVersion();
    } else {
        payload["listVersion"] = -1;
    }
    return doc;
}
