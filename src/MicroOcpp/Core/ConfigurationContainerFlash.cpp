// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Core/ConfigurationContainerFlash.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Debug.h>

#define MAX_CONFIGURATIONS 50

namespace MicroOcpp {

class ConfigurationContainerFlash : public ConfigurationContainer {
private:
    ConfigurationPool container;
    std::shared_ptr<FilesystemAdapter> filesystem;
    uint16_t revisionSum = 0;

    std::vector<std::string> keyPool;
    
    void clearKeyPool(const char *key) {
        keyPool.erase(std::remove_if(keyPool.begin(), keyPool.end(), 
            [key] (std::string& k) {
                return !k.compare(key);
            }));
    }

    bool configurationsUpdated() {
        auto revisionSum_old = revisionSum;

        revisionSum = 0;
        auto& configurations = container.getConfigurations();
        for (size_t i = 0; i < configurations.size(); i++) {
            revisionSum += configurations[i]->getValueRevision();
        }

        return revisionSum == revisionSum_old;
    }
public:
    ConfigurationContainerFlash(std::shared_ptr<FilesystemAdapter> filesystem, const char *filename, bool accessible) :
            ConfigurationContainer(filename, accessible), filesystem(filesystem) { }

    ~ConfigurationContainerFlash() = default;

    bool load() override {

        if (!filesystem) {
            return false;
        }

        auto& configurations = container.getConfigurations();

        size_t file_size = 0;
        if (filesystem->stat(getFilename(), &file_size) != 0 // file does not exist
                || file_size == 0) {                         // file exists, but empty
            MOCPP_DBG_DEBUG("Populate FS: create configuration file");
            return save();
        }

        auto doc = FilesystemUtils::loadJson(filesystem, getFilename());
        if (!doc) {
            MOCPP_DBG_ERR("failed to load %s", getFilename());
            return false;
        }

        JsonObject root = doc->as<JsonObject>();

        JsonObject configHeader = root["head"];

        if (strcmp(configHeader["content-type"] | "Invalid", "ocpp_config_file") &&
                strcmp(configHeader["content-type"] | "Invalid", "ao_configuration_file")) { //backwards-compatibility
            MOCPP_DBG_ERR("Unable to initialize: unrecognized configuration file format");
            return false;
        }

        if (strcmp(configHeader["version"] | "Invalid", "2.0") &&
                strcmp(configHeader["version"] | "Invalid", "1.1")) { //backwards-compatibility
            MOCPP_DBG_ERR("Unable to initialize: unsupported version");
            return false;
        }
        
        JsonArray configurationsArray = root["configurations"];
        if (configurationsArray.size() > MAX_CONFIGURATIONS) {
            MOCPP_DBG_ERR("Unable to initialize: configurations_len is too big (=%zu)", configurationsArray.size());
            return false;
        }

        for (JsonObject stored : configurationsArray) {
            TConfig type;
            if (!deserializeTConfig(stored["type"] | "_Undefined", type)) {
                MOCPP_DBG_ERR("corrupt config");
                continue;
            }

            const char *key = stored["key"] | "";
            if (!*key) {
                MOCPP_DBG_ERR("corrupt config");
                continue;
            }

            if (!stored.containsKey("value")) {
                MOCPP_DBG_ERR("corrupt config");
                continue;
            }
            
            std::string key_pooled;

            Configuration *config = container.getConfiguration(key);
            if (!config || config->getType() != type) {
                key_pooled = key;
                config = nullptr;
            }

            switch (type) {
                case TConfig::Int: {
                    if (!stored["value"].is<int>()) {
                        MOCPP_DBG_ERR("corrupt config");
                        continue;
                    }
                    int value = stored["value"] | 0;
                    if (config) {
                        //config already exists
                        config->setInt(value);
                    } else {
                        //create new config
                        config = container.declareConfiguration<int>(key_pooled.c_str(), value).get();
                    }
                    break;
                }
                case (TConfig::Bool):
                    if (!stored["value"].is<bool>()) {
                        MOCPP_DBG_ERR("corrupt config");
                        continue;
                    }
                    bool value = stored["value"] | false;
                    if (config) {
                        //config already exists
                        config->setBool(value);
                    } else {
                        //create new config
                        config = container.declareConfiguration<bool>(key_pooled.c_str(), value).get();
                    }
                    break;
                case (TConfig::String):
                    if (!stored["value"].is<const char*>()) {
                        MOCPP_DBG_ERR("corrupt config");
                        continue;
                    }
                    const char *value = stored["value"] | "";
                    if (config) {
                        //config already exists
                        config->setBool(value);
                    } else {
                        //create new config
                        config = container.declareConfiguration<bool>(key_pooled.c_str(), value).get();
                    }
                    break;
            }

            if (config) {
                //success

                if (!key_pooled.empty()) {
                    //allocated key, need to store
                    keyPool.push_back(std::string(key));
                }
            } else {
                MOCPP_DBG_ERR("OOM: %s", key);
                (void)0;
            }
        }

        configurationsUpdated();

        MOCPP_DBG_DEBUG("Initialization finished");
        return true;
    }

    bool save() override {

        if (!filesystem) {
            return false;
        }

        if (!configurationsUpdated()) {
            return true; //nothing to be done
        }

        auto& configurations = container.getConfigurations();

        size_t jsonCapacity = 2 * JSON_OBJECT_SIZE(2); //head + configurations + head payload
        jsonCapacity += JSON_ARRAY_SIZE(configurations.size()); //configurations array
        jsonCapacity += configurations.size() * JSON_OBJECT_SIZE(3); //config entries in array

        if (jsonCapacity > MOCPP_MAX_JSON_CAPACITY) {
            MOCPP_DBG_ERR("configs JSON exceeds maximum capacity (%s, %zu entries). Crop configs file (by FCFS)", getFilename(), configurations.size());
            jsonCapacity = MOCPP_MAX_JSON_CAPACITY;
        }

        DynamicJsonDocument doc {jsonCapacity};
        JsonObject head = doc.createNestedObject("head");
        head["content-type"] = "ocpp_config_file";
        head["version"] = "2.0";

        JsonArray configurationsArray = doc.createNestedArray("configurations");

        size_t trackCapacity = 0;

        for (size_t i = 0; i < configurations.size(); i++) {
            auto& config = *configurations[i];
            if (!config.getKey()) {
                MOCPP_DBG_ERR("corrupt config");
                continue;
            }

            size_t entryCapacity = JSON_OBJECT_SIZE(3) + (JSON_ARRAY_SIZE(2) - JSON_ARRAY_SIZE(1));
            if (trackCapacity + entryCapacity > MOCPP_MAX_JSON_CAPACITY) {
                break;
            }

            trackCapacity += entryCapacity;

            auto stored = configurationsArray.createNestedObject();

            stored["type"] = serializeTConfig(config.getType());
            stored["key"] = config.getKey();
            
            switch (config.getType()) {
                case TConfig::Int:
                    stored["value"] = config.getInt();
                    break;
                case TConfig::Bool:
                    stored["value"] = config.getBool();
                    break;
                case TConfig::String:
                    if (!config.getString()) {
                        MOCPP_DBG_ERR("corrupt config");
                        configurationsArray.remove(configurationsArray.size());
                        break;
                    }
                    stored["value"] = config.getString();
                    break;
            }
        }

        bool success = FilesystemUtils::storeJson(filesystem, getFilename(), doc);

        if (success) {
            MOCPP_DBG_DEBUG("Saving configurations finished");
        } else {
            MOCPP_DBG_ERR("could not save configs file: %s", getFilename());
        }

        return success;
    }

    std::shared_ptr<Configuration> declareConfigurationInt(const char *key, int factoryDef, bool readonly = false, bool rebootRequired = false) override {
        auto res = container.declareConfiguration<int>(key, factoryDef, readonly, rebootRequired);
        if (res) res->setKey(key);
        clearKeyPool(key);
        return res;
    }

    std::shared_ptr<Configuration> declareConfigurationBool(const char *key, bool factoryDef, bool readonly = false, bool rebootRequired = false) override {
        auto res = container.declareConfiguration<bool>(key, factoryDef, readonly, rebootRequired);
        if (res) res->setKey(key);
        clearKeyPool(key);
        return res;
    }

    std::shared_ptr<Configuration> declareConfigurationString(const char *key, const char*factoryDef, bool readonly = false, bool rebootRequired = false) override {
        auto res = container.declareConfiguration<const char*>(key, factoryDef, readonly, rebootRequired);
        if (res) res->setKey(key);
        clearKeyPool(key);
        return res;
    }

    size_t getConfigurationCount() override {
        return container.getConfigurationCount();
    }

    Configuration *getConfiguration(size_t i) override {
        return container.getConfiguration(i);
    }

    Configuration *getConfiguration(const char *key) override {
        return container.getConfiguration(key);
    }

};

std::unique_ptr<ConfigurationContainer> makeConfigurationContainerFlash(std::shared_ptr<FilesystemAdapter> filesystem, const char *filename, bool accessible) {
    return std::unique_ptr<ConfigurationContainer>(new ConfigurationContainerFlash(filesystem, filename, accessible));
}

} //end namespace MicroOcpp
