// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Operations/ChangeConfiguration.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Debug.h>

#include <cctype> //for tolower

using MicroOcpp::Ocpp16::ChangeConfiguration;

ChangeConfiguration::ChangeConfiguration() {
  
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

    int nDigits = 0, nNonDigits = 0, nDots = 0, nSign = 0; //"-1.234" has 4 digits, 0 nonDigits, 1 dot and 1 sign. Don't allow comma as seperator. Don't allow e-expressions (e.g. 1.23e-7)
    for (const char *c = value; *c; ++c) {
        if (*c >= '0' && *c <= '9') {
            //int interpretation
            if (nDots == 0) { //only append number if before floating point
                nDigits++;
                numInt *= 10;
                numInt += *c - '0';
            }
        } else if (*c == '.') {
            nDots++;
        } else if (c == value && *c == '-') {
            nSign++;
        } else {
            nNonDigits++;
        }
    }

    if (nSign == 1) {
        numInt = -numInt;
    }

    int INT_MAXDIGITS; //plausibility check: this allows a numerical range of (-999,999,999 to 999,999,999), or (-9,999 to 9,999) respectively
    if (sizeof(int) >= 4UL)
        INT_MAXDIGITS = 9;
    else
        INT_MAXDIGITS = 4;

    if (nNonDigits > 0 || nDigits == 0 || nSign > 1 || nDots > 1) {
        convertibleInt = false;
    }

    if (nDigits > INT_MAXDIGITS) {
        MO_DBG_DEBUG("Possible integer overflow: key = %s, value = %s", key, value);
        convertibleInt = false;
    }

    if (tolower(value[0]) == 't' && tolower(value[1]) == 'r' && tolower(value[2]) == 'u' && tolower(value[3]) == 'e' && !value[4]) {
        numBool = true;
    } else if (tolower(value[0]) == 'f' && tolower(value[1]) == 'a' && tolower(value[2]) == 'l' && tolower(value[3]) == 's' && tolower(value[4]) == 'e' && !value[5]) {
        numBool = false;
    } else if (convertibleInt) {
        numBool = numInt != 0;
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

std::unique_ptr<DynamicJsonDocument> ChangeConfiguration::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
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
