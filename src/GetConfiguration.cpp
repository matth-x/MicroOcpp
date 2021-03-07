// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <Variants.h>

#include "GetConfiguration.h"
#include "Configuration.h"

GetConfiguration::GetConfiguration() {
    keys = LinkedList<String>();
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

    std::shared_ptr<LinkedList<std::shared_ptr<AbstractConfiguration>>> configurationKeys;
    LinkedList<String> unknownKeys = LinkedList<String>();

    if (keys.size() <= 0){ //return all existing keys
        configurationKeys = getAllConfigurations();
    } else { //only return keys that were searched using the "key" parameter
        configurationKeys = std::make_shared<LinkedList<std::shared_ptr<AbstractConfiguration>>>();
        for (int i = 0; i < keys.size(); i++) {
            std::shared_ptr<AbstractConfiguration> entry = getConfiguration(keys.get(i).c_str());
            if (entry)
                configurationKeys->add(entry);
            else
                unknownKeys.add(keys.get(i).c_str());
        }
    }

    size_t capacity = 0;
    LinkedList<std::shared_ptr<DynamicJsonDocument>> configurationKeysJson = LinkedList<std::shared_ptr<DynamicJsonDocument>>();

    for (int i = 0; i < configurationKeys->size(); i++) {
        std::shared_ptr<DynamicJsonDocument> entry = configurationKeys->get(i)->toJsonOcppMsgEntry();
        if (entry) {
            configurationKeysJson.add(entry);
            capacity += entry->capacity();
        }
    }

    for (int i = 0; i < unknownKeys.size(); i++) {
        capacity += unknownKeys.get(i).length() + 1;
    }

    capacity += JSON_OBJECT_SIZE(2) //configurationKey, unknownKey
                + JSON_ARRAY_SIZE(configurationKeysJson.size())
                + JSON_ARRAY_SIZE(unknownKeys.size());
    
    DynamicJsonDocument* doc = new DynamicJsonDocument(capacity);

    JsonObject payload = doc->to<JsonObject>();
    
    JsonArray jsonConfigurationKey = payload.createNestedArray("configurationKey");
    for (int i = 0; i < configurationKeys->size(); i++) {
        jsonConfigurationKey.add(configurationKeysJson.get(i)->as<JsonObject>());
    }

    if (unknownKeys.size() > 0) {
        if (DEBUG_OUT) Serial.print(F("My unknown keys are: \n"));
        JsonArray jsonUnknownKey = payload.createNestedArray("unknownKey");
        for (int i = 0; i < unknownKeys.size(); i++) {
            if (DEBUG_OUT) Serial.println(unknownKeys.get(i));
            jsonUnknownKey.add(unknownKeys.get(i));
        }
    }

    return doc;
}
