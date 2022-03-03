// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/MessagesV16/ClearCache.h>
#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::Ocpp16::ClearCache;

ClearCache::ClearCache() {
  
}

const char* ClearCache::getOcppOperationType(){
    return "ClearCache";
}

void ClearCache::processReq(JsonObject payload) {
    AO_DBG_WARN("Authorization Cache not supported - ClearCache is without effect");
}

std::unique_ptr<DynamicJsonDocument> ClearCache::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = "Accepted"; //"Accepted", because the intended postcondition is true
    return doc;
}
