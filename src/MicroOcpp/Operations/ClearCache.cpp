// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/ClearCache.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::ClearCache;
using MicroOcpp::MemJsonDoc;

ClearCache::ClearCache(std::shared_ptr<FilesystemAdapter> filesystem) : AllocOverrider("v16.Operation.", "ClearCache"), filesystem(filesystem) {
  
}

const char* ClearCache::getOperationType(){
    return "ClearCache";
}

void ClearCache::processReq(JsonObject payload) {
    MO_DBG_WARN("Clear transaction log (Authorization Cache not supported)");

    if (!filesystem) {
        //no persistency - nothing to do
        return;
    }

    success = FilesystemUtils::remove_if(filesystem, [] (const char *fname) -> bool {
        return !strncmp(fname, "sd", strlen("sd")) ||
               !strncmp(fname, "tx", strlen("tx")) ||
               !strncmp(fname, "op", strlen("op"));
    });
}

std::unique_ptr<MemJsonDoc> ClearCache::createConf(){
    auto doc = makeMemJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    if (success) {
        payload["status"] = "Accepted"; //"Accepted", because the intended postcondition is true
    } else {
        payload["status"] = "Rejected";
    }
    return doc;
}
