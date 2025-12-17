// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/ChangeConfiguration.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Debug.h>

#include <cctype> //for tolower

using MicroOcpp::Ocpp16::ChangeConfiguration;
using MicroOcpp::JsonDoc;

ChangeConfiguration::ChangeConfiguration() : MemoryManaged("v16.Operation.", "ChangeConfiguration") {
  
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

    auto configuration = getConfigurationPublic(key);

    if (!configuration) {
        //configuration not found or hidden configuration
        notSupported = true;
        return;
    }

    if (configuration->isReadOnly()) {
        MO_DBG_WARN("Trying to override readonly value");
        readOnly = true;
        return;
    }

    //write config

    /*
    * Try to interpret input as number
    */

    bool convertibleInt = true;
    int numInt = 0;
    bool convertibleBool = true;
    bool numBool = false;

    // Use safe string-to-int conversion with overflow protection
    if (!safeStringToInt(value, &numInt)) {
        convertibleInt = false;
    }

    if (tolower(value[0]) == 't' && tolower(value[1]) == 'r' && tolower(value[2]) == 'u' && tolower(value[3]) == 'e' && !value[4]) {
        numBool = true;
    } else if (tolower(value[0]) == 'f' && tolower(value[1]) == 'a' && tolower(value[2]) == 'l' && tolower(value[3]) == 's' && tolower(value[4]) == 'e' && !value[5]) {
        numBool = false;
    } else {
        convertibleBool = false;
    }

    //check against optional validator

    auto validator = getConfigurationValidator(key);
    if (validator && !(*validator)(value)) {
        //validator exists and validation fails
        reject = true;
        MO_DBG_WARN("validation failed for key=%s value=%s", key, value);
        return;
    }

    //Store (parsed) value to Config

    if (configuration->getType() == TConfig::Int && convertibleInt) {
        configuration->setInt(numInt);
    } else if (configuration->getType() == TConfig::Bool && convertibleBool) {
        configuration->setBool(numBool);
    } else if (configuration->getType() == TConfig::String) {
        configuration->setString(value);
    } else {
        reject = true;
        MO_DBG_WARN("Value has incompatible type");
        return;
    }

    if (!configuration_save()) {
        MO_DBG_ERR("could not write changes to flash");
        errorCode = "InternalError";
        return;
    }

    if (configuration->isRebootRequired()) {
        rebootRequired = true;
    }
}

std::unique_ptr<JsonDoc> ChangeConfiguration::createConf(){
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    if (notSupported) {
        payload["status"] = "NotSupported";
    } else if (reject || readOnly) {
        payload["status"] = "Rejected";
    } else if (rebootRequired) {
        payload["status"] = "RebootRequired";
    } else {
        payload["status"] = "Accepted";
    }
    return doc;
}
