// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/GetConfiguration.h>
#include <MicroOcpp/Model/Configuration/ConfigurationService.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16

using namespace MicroOcpp;
using namespace MicroOcpp::Ocpp16;

GetConfiguration::GetConfiguration(ConfigurationService& configService) :
        MemoryManaged("v16.Operation.", "GetConfiguration"),
        configService(configService),
        configurations(makeVector<Configuration*>(getMemoryTag())),
        unknownKeys(makeVector<char*>(getMemoryTag())) {

}

GetConfiguration::~GetConfiguration() {
    for (size_t i = 0; i < unknownKeys.size(); i++) {
        MO_FREE(unknownKeys[i]);
        unknownKeys[i] = nullptr;
    }
}

const char* GetConfiguration::getOperationType(){
    return "GetConfiguration";
}

void GetConfiguration::processReq(JsonObject payload) {

    JsonArray keysArray = payload["key"];
    const char **keys = nullptr;
    size_t keysSize = keysArray.size();
    if (keysSize > 0) {
        keys = static_cast<const char**>(MO_MALLOC(getMemoryTag(), sizeof(const char**) * keysSize));
        if (!keys) {
            MO_DBG_ERR("OOM");
            errorCode = "InternalError";
            errorDescription = "Query too big. Try fewer keys";
            goto exit;
        }
        memset(keys, 0, sizeof(const char**) * keysSize);
    }

    for (size_t i = 0; i < keysArray.size(); i++) {
        const char *key = keysArray[i].as<const char*>();
        if (!key || !*key) {
            errorCode = "FormationViolation";
            goto exit;
        }

        keys[i] = key;
    }

    if (!configService.getConfiguration(keys, keysSize, configurations, unknownKeys)) {
        errorCode = "InternalError";
        errorDescription = "Query too big. Try fewer keys";
        goto exit;
    }

exit:
    MO_FREE(keys);
    keys = nullptr;
    keysSize = 0;
}

std::unique_ptr<JsonDoc> GetConfiguration::createConf(){

    const size_t VALUE_BUFSIZE = 30;

    //capacity of the resulting document
    size_t jcapacity = JSON_OBJECT_SIZE(2); //document root: configurationKey, unknownKey

    jcapacity += JSON_ARRAY_SIZE(configurations.size()) + configurations.size() * JSON_OBJECT_SIZE(3); //configurationKey: [{"key":...},{"key":...}]
    for (size_t i = 0; i < configurations.size(); i++) {
        auto config = configurations[i];
        //need to store ints by copied string: measure necessary capacity
        if (config->getType() == Configuration::Type::Int) {
            char vbuf [VALUE_BUFSIZE];
            auto ret = snprintf(vbuf, sizeof(vbuf), "%i", config->getInt());
            if (ret < 0 || (size_t)ret >= sizeof(vbuf)) {
                MO_DBG_ERR("snprintf: %i", ret);
                continue;
            }
            jcapacity += (size_t)ret + 1;
        }
    }

    jcapacity += JSON_ARRAY_SIZE(unknownKeys.size());
    
    MO_DBG_DEBUG("GetConfiguration capacity: %zu", jcapacity);

    std::unique_ptr<JsonDoc> doc;

    if (jcapacity <= MO_MAX_JSON_CAPACITY) {
        doc = makeJsonDoc(getMemoryTag(), jcapacity);
    }

    if (!doc || doc->capacity() < jcapacity) {
        if (doc) {
            MO_DBG_ERR("OOM");
        }

        errorCode = "InternalError";
        errorDescription = "Query too big. Try fewer keys";
        return nullptr;
    }

    JsonObject payload = doc->to<JsonObject>();
    
    JsonArray jsonConfigurationKey = payload.createNestedArray("configurationKey");
    for (size_t i = 0; i < configurations.size(); i++) {
        auto config = configurations[i];
        char vbuf [VALUE_BUFSIZE];
        const char *v = "";
        switch (config->getType()) {
            case Configuration::Type::Int: {
                auto ret = snprintf(vbuf, VALUE_BUFSIZE, "%i", config->getInt());
                if (ret < 0 || ret >= VALUE_BUFSIZE) {
                    MO_DBG_ERR("value error");
                    continue;
                }
                v = vbuf;
                break;
            }
            case Configuration::Type::Bool:
                v = config->getBool() ? "true" : "false";
                break;
            case Configuration::Type::String:
                v = config->getString();
                break;
        }

        JsonObject jconfig = jsonConfigurationKey.createNestedObject();
        jconfig["key"] = config->getKey();
        jconfig["readonly"] = config->getMutability() == Mutability::ReadOnly;
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
        for (size_t i = 0; i < unknownKeys.size(); i++) {
            auto key = unknownKeys[i];
            MO_DBG_DEBUG("Unknown key: %s", key);
            jsonUnknownKey.add((const char*)key); //force zero-copy mode
        }
    }

    return doc;
}

#endif //MO_ENABLE_V16
