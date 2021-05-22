// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <Variants.h>

#include <ArduinoOcpp/MessagesV16/GetConfiguration.h>
#include <ArduinoOcpp/Core/Configuration.h>

using ArduinoOcpp::Ocpp16::GetConfiguration;

GetConfiguration::GetConfiguration() {
    
}

GetConfiguration::~GetConfiguration() {
    keys.clear();
}

const char* GetConfiguration::getOcppOperationType(){
    return "GetConfiguration";
}

void GetConfiguration::processReq(JsonObject payload) {

    JsonArray requestedKeys = payload["key"];
    for (uint16_t i = 0; i < requestedKeys.size(); i++) {
        keys.add(requestedKeys[i].as<String>());
    }
}

DynamicJsonDocument* GetConfiguration::createConf(){

    std::shared_ptr<std::vector<std::shared_ptr<AbstractConfiguration>>> configurationKeys;
    std::vector<String> unknownKeys;

    if (keys.size() <= 0){ //return all existing keys
        configurationKeys = getAllConfigurations();
    } else { //only return keys that were searched using the "key" parameter
        for (int i = 0; i < keys.size(); i++) {
            std::shared_ptr<AbstractConfiguration> entry = getConfiguration(keys.get(i).c_str());
            if (entry)
                configurationKeys->push_back(entry);
            else
                unknownKeys.push_back(keys.get(i).c_str());
        }
    }

    size_t capacity = 0;
    std::vector<std::shared_ptr<DynamicJsonDocument>> configurationKeysJson;

    for (auto confKey = configurationKeys->begin(); confKey != configurationKeys->end(); confKey++) {
        std::shared_ptr<DynamicJsonDocument> entry = (*confKey)->toJsonOcppMsgEntry();
        if (entry) {
            configurationKeysJson.push_back(entry);
            capacity += entry->capacity();
        }
    }

    for (auto unknownKey = unknownKeys.begin(); unknownKey != unknownKeys.end(); unknownKey++) {
        capacity += unknownKey->length() + 1;
    }

    capacity += JSON_OBJECT_SIZE(2) //configurationKey, unknownKey
                + JSON_ARRAY_SIZE(configurationKeysJson.size())
                + JSON_ARRAY_SIZE(unknownKeys.size());
    
    DynamicJsonDocument* doc = new DynamicJsonDocument(capacity);

    JsonObject payload = doc->to<JsonObject>();
    
    JsonArray jsonConfigurationKey = payload.createNestedArray("configurationKey");
    for (int i = 0; i < configurationKeys->size(); i++) {
        jsonConfigurationKey.add(configurationKeysJson.at(i)->as<JsonObject>());
    }

    if (unknownKeys.size() > 0) {
        if (DEBUG_OUT) Serial.print(F("My unknown keys are: \n"));
        JsonArray jsonUnknownKey = payload.createNestedArray("unknownKey");
        for (auto unknownKey = unknownKeys.begin(); unknownKey != unknownKeys.end(); unknownKey++) {
            if (DEBUG_OUT) Serial.println(*unknownKey);
            jsonUnknownKey.add(*unknownKey);
        }
    }

    return doc;
}
