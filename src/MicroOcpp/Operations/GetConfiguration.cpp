// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Operations/GetConfiguration.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::GetConfiguration;

GetConfiguration::GetConfiguration() {

}

const char* GetConfiguration::getOperationType(){
    return "GetConfiguration";
}

void GetConfiguration::processReq(JsonObject payload) {

    JsonArray requestedKeys = payload["key"];
    for (size_t i = 0; i < requestedKeys.size(); i++) {
        keys.push_back(requestedKeys[i].as<std::string>());
    }
}

std::unique_ptr<DynamicJsonDocument> GetConfiguration::createConf(){

    std::vector<Configuration*> configurations;
    std::vector<const char*> unknownKeys;

    auto containers = getConfigurationContainersPublic();

    if (keys.empty()) {
        //return all existing keys
        for (auto container : containers) {
            for (size_t i = 0; i < container->size(); i++) {
                if (!container->getConfiguration(i)->getKey()) {
                    MO_DBG_ERR("invalid config");
                    continue;
                }
                configurations.push_back(container->getConfiguration(i));
            }
        }
    } else {
        //only return keys that were searched using the "key" parameter
        for (auto& key : keys) {
            Configuration *res = nullptr;
            for (auto container : containers) {
                if ((res = container->getConfiguration(key.c_str()).get())) {
                    break;
                }
            }

            if (res) {
                configurations.push_back(res);
            } else {
                unknownKeys.push_back(key.c_str());
            }
        }
    }

    #define VALUE_BUFSIZE 30

    //capacity of the resulting document
    size_t jcapacity = JSON_OBJECT_SIZE(2); //document root: configurationKey, unknownKey

    jcapacity += JSON_ARRAY_SIZE(configurations.size()) + configurations.size() * JSON_OBJECT_SIZE(3); //configurationKey: [{"key":...},{"key":...}]
    for (auto config : configurations) {
        //need to store ints by copied string: measure necessary capacity
        if (config->getType() == TConfig::Int) {
            char vbuf [VALUE_BUFSIZE];
            auto ret = snprintf(vbuf, VALUE_BUFSIZE, "%i", config->getInt());
            if (ret < 0 || ret >= VALUE_BUFSIZE) {
                continue;
            }
            jcapacity += ret + 1;
        }
    }

    jcapacity += JSON_ARRAY_SIZE(unknownKeys.size());
    
    MO_DBG_DEBUG("GetConfiguration capacity: %zu", jcapacity);

    std::unique_ptr<DynamicJsonDocument> doc;

    if (jcapacity <= MO_MAX_JSON_CAPACITY) {
        doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(jcapacity));
    }

    if (!doc || doc->capacity() < jcapacity) {
        if (doc) {MO_DBG_ERR("OOM");(void)0;}

        errorCode = "InternalError";
        errorDescription = "Query too big. Try fewer keys";
        return nullptr;
    }

    JsonObject payload = doc->to<JsonObject>();
    
    JsonArray jsonConfigurationKey = payload.createNestedArray("configurationKey");
    for (auto config : configurations) {
        char vbuf [VALUE_BUFSIZE];
        const char *v = "";
        switch (config->getType()) {
            case TConfig::Int: {
                auto ret = snprintf(vbuf, VALUE_BUFSIZE, "%i", config->getInt());
                if (ret < 0 || ret >= VALUE_BUFSIZE) {
                    MO_DBG_ERR("value error");
                    continue;
                }
                v = vbuf;
                break;
            }
            case TConfig::Bool:
                v = config->getBool() ? "true" : "false";
                break;
            case TConfig::String:
                v = config->getString();
                break;
        }

        JsonObject jconfig = jsonConfigurationKey.createNestedObject();
        jconfig["key"] = config->getKey();
        jconfig["readonly"] = config->isReadOnly();
        if (v == vbuf) {
            //value points to buffer on stack, needs to be copied into JSON memory pool
            jconfig["value"] = (char*) v;
        } else {
            //value is static, no-copy mode
            jconfig["value"] = v;
        }
    }

    if (!unknownKeys.empty()) {
        JsonArray jsonUnknownKey = payload.createNestedArray("unknownKey");
        for (auto key : unknownKeys) {
            MO_DBG_DEBUG("Unknown key: %s", key)
            jsonUnknownKey.add(key);
        }
    }

    return doc;
}
