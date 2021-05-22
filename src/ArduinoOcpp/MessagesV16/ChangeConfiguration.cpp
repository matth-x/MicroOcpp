// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/MessagesV16/ChangeConfiguration.h>
#include <ArduinoOcpp/Core/Configuration.h>

#include <Variants.h>

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
        Serial.print(F("[ChangeConfiguration] Could not read key!\n"));
        return;
    }

    JsonVariant value = payload["value"];
    if (value.isNull()) {
        err = true;
        Serial.print(F("[ChangeConfiguration] Message is lacking value!\n"));
        return;
    }

    std::shared_ptr<AbstractConfiguration> configuration = getConfiguration(key.c_str());

    if (configuration) {
        if (!configuration->permissionRemotePeerCanWrite()) {
            readOnly = true;
            Serial.print(F("[ChangeConfiguration] Trying to override readonly value!\n"));
            return;
        }

        if (!strcmp(configuration->getSerializedType(), SerializedType<int>::get()) && value.is<int>()) {
            std::shared_ptr<Configuration<int>> configurationConcrete = std::static_pointer_cast<Configuration<int>>(configuration);
            *configurationConcrete = value.as<int>();
        } else if (!strcmp(configuration->getSerializedType(), SerializedType<float>::get()) && value.is<float>()) {
            std::shared_ptr<Configuration<float>> configurationConcrete = std::static_pointer_cast<Configuration<float>>(configuration);
            *configurationConcrete = value.as<float>();
        } else if (!strcmp(configuration->getSerializedType(), SerializedType<const char *>::get()) && value.is<const char *>()) {
            std::shared_ptr<Configuration<const char *>> configurationConcrete = std::static_pointer_cast<Configuration<const char *>>(configuration);
            *configurationConcrete = value.as<const char *>();
//        } else if (!strcmp(configuration->getSerializedType(), SerializedType<String>::get()) && value.is<String>()) {
//            std::shared_ptr<Configuration<String>> configurationConcrete = std::static_pointer_cast<Configuration<String>>(configuration);
//            *configurationConcrete = value.as<String>();
        } else {
            err = true;
            Serial.print(F("[ChangeConfiguration] Value has incompatible type!\n"));
            return;
        }
    } else {
        //configuration does not exist yet. Create new entry

        if (value.is<int>()) { //Not enough. SteVe always sends numerical values as strings. Must also handle this
            configuration = std::static_pointer_cast<AbstractConfiguration>(declareConfiguration<int>(key.c_str(), value.as<int>()));
        } else if (value.is<float>()) {
            configuration = std::static_pointer_cast<AbstractConfiguration>(declareConfiguration<float>(key.c_str(), value.as<float>()));
        } else if (value.is<const char *>()) { //SteVe sends numerical values as strings. Check for that first

            const char *value_string = value.as<const char *>();
            size_t value_buffsize = strlen(value_string) + 1;

            /*
            * Try to interpret input as number
            */

            int nDigits = 0, nNonDigits = 0, nDots = 0, nSign = 0; //"-1.234" has 4 digits, 0 nonDigits, 1 dot and 1 sign. Don't allow comma as seperator. Don't allow e-expressions (e.g. 1.23e-7)
            int numInt = 0;
            float numFloat = 0.f;
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
            if (sizeof(int) == 4UL)
                INT_MAXDIGITS = 9;
            else
                INT_MAXDIGITS = 4;

            bool isString = false;

            if (nNonDigits == 0 && nDigits > 0 && nSign <= 1 && nDots == 0) {
                //integer
                if (nDigits > INT_MAXDIGITS) {
                    Serial.print(F("[ChangeConfiguration] Integer overflow! key = "));
                    Serial.print(key.c_str());
                    Serial.print(F(", value = "));
                    Serial.println(value_string);
                    err = true;
                    return;
                } else {
                    if (nSign == 1) {
                        int neg = -numInt;
                        numInt = neg; 
                    }
                    configuration = std::static_pointer_cast<AbstractConfiguration>(declareConfiguration<int>(key.c_str(), numInt));
                }
            } else if (nNonDigits == 0 && nDigits > 0 && nSign <= 1 && nDots == 1) {
                //float
                // no overflow check
                numFloat /= numFloatTranslate; // "1." <-- numFlTrans = 1.f; "-1.234" <-- numFlTrans = 1000.f

                if (nSign == 1) {
                    numFloat *= -1.f;
                }

                configuration = std::static_pointer_cast<AbstractConfiguration>(declareConfiguration<float>(key.c_str(), numFloat));
            } else {
                //It's really a string
                configuration = std::static_pointer_cast<AbstractConfiguration>(declareConfiguration<const char *>(key.c_str(), value_string));
            }
        }
    } //end if (configuration)
    
    if (!configuration) {
        err = true;
        Serial.print(F("[ChangeConfiguration] Could not deserialize value or set configuration!\n"));
        return;
    }

    //success
    configuration_save(); //TODO encapsulate in AbstractConfiguration: will split configurations in multiple files. Only write back modified data

    if (configuration->requiresRebootWhenChanged()) {
        rebootRequired = true;
    }

    //success
}

DynamicJsonDocument* ChangeConfiguration::createConf(){
  DynamicJsonDocument* doc = new DynamicJsonDocument(JSON_OBJECT_SIZE(1));
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
