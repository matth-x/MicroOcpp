// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#include <MicroOcpp/Operations/ChangeConfiguration.h>
#include <MicroOcpp/Model/Configuration/ConfigurationService.h>
#include <MicroOcpp/Debug.h>

#include <ctype.h> //for tolower

#if MO_ENABLE_V16

using namespace MicroOcpp;
using namespace MicroOcpp::v16;

ChangeConfiguration::ChangeConfiguration(ConfigurationService& configService) : MemoryManaged("v16.Operation.", "ChangeConfiguration"), configService(configService) {
  
}

const char* ChangeConfiguration::getOperationType(){
    return "ChangeConfiguration";
}

void ChangeConfiguration::processReq(JsonObject payload) {
    const char *key = payload["key"] | "";
    if (!*key) {
        errorCode = "FormationViolation";
        MO_DBG_WARN("Could not read key");
        return;
    }

    if (!payload["value"].is<const char *>()) {
        errorCode = "FormationViolation";
        MO_DBG_WARN("Message is lacking value");
        return;
    }

    const char *value = payload["value"];

    status = configService.changeConfiguration(key, value);
}

std::unique_ptr<JsonDoc> ChangeConfiguration::createConf(){
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    const char *statusStr = "";
    switch (status) {
        case ConfigurationStatus::Accepted:
            statusStr = "Accepted";
            break;
        case ConfigurationStatus::Rejected:
            statusStr = "Rejected";
            break;
        case ConfigurationStatus::RebootRequired:
            statusStr = "RebootRequired";
            break;
        case ConfigurationStatus::NotSupported:
            statusStr = "NotSupported";
            break;
    }
    payload["status"] = statusStr;
    return doc;
}

#endif //MO_ENABLE_V16
