// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Operations/ClearCache.h>
#include <ArduinoOcpp/Core/FilesystemUtils.h>
#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::Ocpp16::ClearCache;

ClearCache::ClearCache(std::shared_ptr<FilesystemAdapter> filesystem) : filesystem(filesystem) {
  
}

const char* ClearCache::getOperationType(){
    return "ClearCache";
}

void ClearCache::processReq(JsonObject payload) {
    AO_DBG_WARN("Clear transaction log (Authorization Cache not supported)");

    if (!filesystem) {
        //no persistency - nothing to do
        return;
    }

    success &= FilesystemUtils::remove_all(filesystem, "sd");
    success &= FilesystemUtils::remove_all(filesystem, "tx");
    success &= FilesystemUtils::remove_all(filesystem, "op");
}

std::unique_ptr<DynamicJsonDocument> ClearCache::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    if (success) {
        payload["status"] = "Accepted"; //"Accepted", because the intended postcondition is true
    } else {
        payload["status"] = "Rejected";
    }
    return doc;
}
