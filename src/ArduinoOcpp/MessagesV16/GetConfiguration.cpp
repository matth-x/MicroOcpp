// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <Variants.h>

#include <ArduinoOcpp/MessagesV16/GetConfiguration.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::Ocpp16::GetConfiguration;

GetConfiguration::GetConfiguration() {

}

const char* GetConfiguration::getOcppOperationType(){
    return "GetConfiguration";
}

void GetConfiguration::processReq(JsonObject payload) {

    JsonArray requestedKeys = payload["key"];
    for (size_t i = 0; i < requestedKeys.size(); i++) {
        keys.push_back(requestedKeys[i].as<std::string>());
    }
}

std::unique_ptr<DynamicJsonDocument> GetConfiguration::createConf(){

    std::shared_ptr<std::vector<std::shared_ptr<AbstractConfiguration>>> configurationKeys;
    std::vector<std::string> unknownKeys;

    if (keys.size() == 0){ //return all existing keys
        configurationKeys = getAllConfigurations();
    } else { //only return keys that were searched using the "key" parameter
        configurationKeys = std::make_shared<std::vector<std::shared_ptr<AbstractConfiguration>>>();
        for (size_t i = 0; i < keys.size(); i++) {
            std::shared_ptr<AbstractConfiguration> entry = getConfiguration(keys.at(i).c_str());
            if (entry)
                configurationKeys->push_back(entry);
            else
                unknownKeys.push_back(keys.at(i).c_str());
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
    
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(capacity));

    JsonObject payload = doc->to<JsonObject>();
    
    JsonArray jsonConfigurationKey = payload.createNestedArray("configurationKey");
    for (size_t i = 0; i < configurationKeys->size(); i++) {
        jsonConfigurationKey.add(configurationKeysJson.at(i)->as<JsonObject>());
    }

    if (unknownKeys.size() > 0) {
        JsonArray jsonUnknownKey = payload.createNestedArray("unknownKey");
        for (auto unknownKey = unknownKeys.begin(); unknownKey != unknownKeys.end(); unknownKey++) {
            AO_DBG_DEBUG("Unknown key: %s", unknownKey->c_str())
            jsonUnknownKey.add(*unknownKey);
        }
    }

    return doc;
}
