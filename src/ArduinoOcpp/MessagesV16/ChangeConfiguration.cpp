// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/MessagesV16/ChangeConfiguration.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::Ocpp16::ChangeConfiguration;

ChangeConfiguration::ChangeConfiguration() {
  
}

const char* ChangeConfiguration::getOcppOperationType(){
    return "ChangeConfiguration";
}

void ChangeConfiguration::processReq(JsonObject payload) {
    String key = payload["key"];
    if (key.isEmpty()) {
        err = true;
        AO_DBG_WARN("Could not read key");
        return;
    }

    JsonVariant value = payload["value"];
    if (value.isNull()) {
        err = true;
        AO_DBG_WARN("Message is lacking value");
        return;
    }

    /*
     * Delete entry if value field is empty string
     */
    if (value.is<const char *>()) {
        const char *value_string = value.as<const char *>();
        if (value_string == nullptr || value_string[0] == '\0') {
            auto configuration = getConfiguration(key.c_str());
            if (configuration) {
                if (configuration->permissionRemotePeerCanWrite()) {
                    configuration->setToBeRemoved();
                    configuration_save();
                    rebootRequired = true;
                } else {
                    readOnly = true;
                    AO_DBG_WARN("Trying to delete readonly value");
                }
            }
            return; //delete operator but nothing to delete --> ignore operation
        }
    }

    /*
     * Parse value
     */
    bool isInt = false;
    int numInt = -1;
    bool isFloat = false;
    float numFloat = -1.0f;
    bool isString = false;
    const char *value_string = "";

    if (value.is<int>()) { //Not enough. SteVe always sends numerical values as strings. Must also handle this
        isInt = true;
        numInt = value.as<int>();
        //configuration = std::static_pointer_cast<AbstractConfiguration>(declareConfiguration<int>(key.c_str(), value.as<int>()));
    } else if (value.is<float>()) {
        isFloat = true;
        numFloat = value.as<float>();
        //configuration = std::static_pointer_cast<AbstractConfiguration>(declareConfiguration<float>(key.c_str(), value.as<float>()));
    } else if (value.is<const char *>()) { //SteVe sends numerical values as strings. Check for that first
        isString = true;
        value_string = value.as<const char *>();
        size_t value_buffsize = strlen(value_string) + 1;

        /*
        * Try to interpret input as number
        */

        int nDigits = 0, nNonDigits = 0, nDots = 0, nSign = 0; //"-1.234" has 4 digits, 0 nonDigits, 1 dot and 1 sign. Don't allow comma as seperator. Don't allow e-expressions (e.g. 1.23e-7)
        numInt = 0;
        numFloat = 0.f;
        float numFloatTranslate = 1.f;
        for (size_t i = 0; i < value_buffsize - 1; i++) { //exclude terminating \0
            char c = value_string[i];
            if (c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7' || c == '8' || c == '9') {
                nDigits++;
                numInt *= 10;
                numInt += c - '0';
                numFloat *= 10.f;
                numFloat += (float) (c - '0');
                if (nDots != 0) {
                    numFloatTranslate *= 10.f;
                }
            } else if (c == '.') {
                nDots++;
            } else if (i == 0 && c == '-') {
                nSign++;
            } else {
                nNonDigits++;
            }
        }

        int INT_MAXDIGITS; //plausibility check: this allows a numerical range of (-999,999,999 to 999,999,999), or (-9,999 to 9,999) respectively
        if (sizeof(int) >= 4UL)
            INT_MAXDIGITS = 9;
        else
            INT_MAXDIGITS = 4;

        //bool isString = false;

        if (nNonDigits == 0 && nDigits > 0 && nSign <= 1 && nDots == 0) {
            //integer
            if (nDigits > INT_MAXDIGITS) {
                AO_DBG_WARN("Integer overflow! key = %s, value = %s", key.c_str(), value_string);
                err = true;
                return;
            } else {
                if (nSign == 1) {
                    int neg = -numInt;
                    numInt = neg; 
                }
                //configuration = std::static_pointer_cast<AbstractConfiguration>(declareConfiguration<int>(key.c_str(), numInt));
                isInt = true;
            }
        } else if (nNonDigits == 0 && nDigits > 0 && nSign <= 1 && nDots == 1) {
            //float
            // no overflow check
            numFloat /= numFloatTranslate; // "1." <-- numFlTrans = 1.f; "-1.234" <-- numFlTrans = 1000.f

            if (nSign == 1) {
                numFloat *= -1.f;
            }

            //configuration = std::static_pointer_cast<AbstractConfiguration>(declareConfiguration<float>(key.c_str(), numFloat));
            isFloat = true;
        } else {
            //only string
            isString = true;
            //configuration = std::static_pointer_cast<AbstractConfiguration>(declareConfiguration<const char *>(key.c_str(), value_string));
        }
    }

    std::shared_ptr<AbstractConfiguration> configuration = getConfiguration(key.c_str());

    if (configuration) {
        if (!configuration->permissionRemotePeerCanWrite()) {
            readOnly = true;
            AO_DBG_WARN("Trying to override readonly value");
            return;
        }

        if (!strcmp(configuration->getSerializedType(), SerializedType<int>::get()) && isInt) {
            std::shared_ptr<Configuration<int>> configurationConcrete = std::static_pointer_cast<Configuration<int>>(configuration);
            *configurationConcrete = numInt;
        } else if (!strcmp(configuration->getSerializedType(), SerializedType<float>::get()) && isFloat) {
            std::shared_ptr<Configuration<float>> configurationConcrete = std::static_pointer_cast<Configuration<float>>(configuration);
            *configurationConcrete = numFloat;
        } else if (!strcmp(configuration->getSerializedType(), SerializedType<const char *>::get()) && isString) {
            std::shared_ptr<Configuration<const char *>> configurationConcrete = std::static_pointer_cast<Configuration<const char *>>(configuration);
            *configurationConcrete = value_string;
//        } else if (!strcmp(configuration->getSerializedType(), SerializedType<String>::get()) && value.is<String>()) {
//            std::shared_ptr<Configuration<String>> configurationConcrete = std::static_pointer_cast<Configuration<String>>(configuration);
//            *configurationConcrete = value.as<String>();
        } else {
            err = true;
            AO_DBG_WARN("Value has incompatible type");
            return;
        }

        configuration->resetToBeRemovedFlag();

    } else {
        //configuration does not exist yet. Create new entry
        if (isInt) {
            configuration = std::static_pointer_cast<AbstractConfiguration>(declareConfiguration<int>(key.c_str(), numInt));
        } else if (isFloat) {
            configuration = std::static_pointer_cast<AbstractConfiguration>(declareConfiguration<float>(key.c_str(), numFloat));
        } else if (isString) {
            configuration = std::static_pointer_cast<AbstractConfiguration>(declareConfiguration<const char*>(key.c_str(), value_string));
        } else {
            err = true;
            AO_DBG_WARN("Could not parse value");
            return;
        }
    } //end if (configuration)
    
    if (!configuration) {
        err = true;
        AO_DBG_WARN("Could not deserialize value or set configuration");
        return;
    }

    //success
    configuration_save(); //TODO encapsulate in AbstractConfiguration: will split configurations in multiple files. Only write back modified data

    if (configuration->requiresRebootWhenChanged()) {
        rebootRequired = true;
    }

    //success
}

std::unique_ptr<DynamicJsonDocument> ChangeConfiguration::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    if (err || readOnly) {
        payload["status"] = "Rejected";
    } else if (rebootRequired) {
        payload["status"] = "RebootRequired";
    } else {
        payload["status"] = "Accepted";
    }
    return doc;
}
