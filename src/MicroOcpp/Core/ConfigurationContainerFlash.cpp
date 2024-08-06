// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Core/ConfigurationContainerFlash.h>

#include <algorithm>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Debug.h>

#define MAX_CONFIGURATIONS 50

namespace MicroOcpp {

class ConfigurationContainerFlash : public ConfigurationContainer, public AllocOverrider {
private:
    std::vector<std::shared_ptr<Configuration>, Allocator<std::shared_ptr<Configuration>>> configurations;
    std::shared_ptr<FilesystemAdapter> filesystem;
    uint16_t revisionSum = 0;

    bool loaded = false;

    std::vector<char*, Allocator<char*>> keyPool;
    
    void clearKeyPool(const char *key) {
        auto it = keyPool.begin();
        while (it != keyPool.end()) {
            if (!strcmp(*it, key)) {
                MO_DBG_VERBOSE("clear key %s", key);
                MO_FREE(*it);
                it = keyPool.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool configurationsUpdated() {
        auto revisionSum_old = revisionSum;

        revisionSum = 0;
        for (auto& config : configurations) {
            revisionSum += config->getValueRevision();
        }

        return revisionSum != revisionSum_old;
    }
public:
    ConfigurationContainerFlash(std::shared_ptr<FilesystemAdapter> filesystem, const char *filename, bool accessible) :
            ConfigurationContainer(filename, accessible), AllocOverrider(filename), configurations(Allocator<std::shared_ptr<Configuration>>(filename)), filesystem(filesystem), keyPool(Allocator<char*>(filename)) { }

    ~ConfigurationContainerFlash() {
        auto it = keyPool.begin();
        while (it != keyPool.end()) {
            MO_FREE(*it);
        }
    }
    
    bool load() override {

        if (loaded) {
            return true;
        }

        if (!filesystem) {
            return false;
        }

        size_t file_size = 0;
        if (filesystem->stat(getFilename(), &file_size) != 0 // file does not exist
                || file_size == 0) {                         // file exists, but empty
            MO_DBG_DEBUG("Populate FS: create configuration file");
            return save();
        }

        auto doc = FilesystemUtils::loadJson(filesystem, getFilename());
        if (!doc) {
            MO_DBG_ERR("failed to load %s", getFilename());
            return false;
        }

        JsonObject root = doc->as<JsonObject>();

        JsonObject configHeader = root["head"];

        if (strcmp(configHeader["content-type"] | "Invalid", "ocpp_config_file") &&
                strcmp(configHeader["content-type"] | "Invalid", "ao_configuration_file")) { //backwards-compatibility
            MO_DBG_ERR("Unable to initialize: unrecognized configuration file format");
            return false;
        }

        if (strcmp(configHeader["version"] | "Invalid", "2.0") &&
                strcmp(configHeader["version"] | "Invalid", "1.1")) { //backwards-compatibility
            MO_DBG_ERR("Unable to initialize: unsupported version");
            return false;
        }
        
        JsonArray configurationsArray = root["configurations"];
        if (configurationsArray.size() > MAX_CONFIGURATIONS) {
            MO_DBG_ERR("Unable to initialize: configurations_len is too big (=%zu)", configurationsArray.size());
            return false;
        }

        for (JsonObject stored : configurationsArray) {
            TConfig type;
            if (!deserializeTConfig(stored["type"] | "_Undefined", type)) {
                MO_DBG_ERR("corrupt config");
                continue;
            }

            const char *key = stored["key"] | "";
            if (!*key) {
                MO_DBG_ERR("corrupt config");
                continue;
            }

            if (!stored.containsKey("value")) {
                MO_DBG_ERR("corrupt config");
                continue;
            }

            auto config = getConfiguration(key).get();
            if (config && config->getType() != type) {
                MO_DBG_ERR("conflicting type for %s - remove old config", key);
                remove(config);
                config = nullptr;
            }

            switch (type) {
                case TConfig::Int: {
                    if (!stored["value"].is<int>()) {
                        MO_DBG_ERR("corrupt config");
                        continue;
                    }
                    int value = stored["value"] | 0;
                    if (!config) {
                        //create new config
                        config = createConfiguration(TConfig::Int, "").get();
                    }
                    if (config) {
                        config->setInt(value);
                    }
                    break;
                }
                case TConfig::Bool: {
                    if (!stored["value"].is<bool>()) {
                        MO_DBG_ERR("corrupt config");
                        continue;
                    }
                    bool value = stored["value"] | false;
                    if (!config) {
                        //create new config
                        config = createConfiguration(TConfig::Bool, "").get();
                    }
                    if (config) {
                        config->setBool(value);
                    }
                    break;
                }
                case TConfig::String: {
                    if (!stored["value"].is<const char*>()) {
                        MO_DBG_ERR("corrupt config");
                        continue;
                    }
                    const char *value = stored["value"] | "";
                    if (!config) {
                        //create new config
                        config = createConfiguration(TConfig::String, "").get();
                    }
                    if (config) {
                        config->setString(value);
                    }
                    break;
                }
            }

            if (config) {
                //success

                if (!config->getKey() || !*config->getKey()) {
                    //created new config, allocate and keep key for config object
                    size_t size = strlen(key) + 1;
                    char *key_pooled = static_cast<char*>(MO_MALLOC(key, size));
                    if (!key_pooled) {
                        MO_DBG_ERR("OOM: %s", key);
                        remove(config);
                        return false;
                    }
                    snprintf(key_pooled, size, "%s", key);
                    keyPool.push_back(key_pooled);
                }
            } else {
                MO_DBG_ERR("OOM: %s", key);
            }
        }

        configurationsUpdated();

        MO_DBG_DEBUG("Initialization finished");
        loaded = true;
        return true;
    }

    bool save() override {

        if (!filesystem) {
            return false;
        }

        if (!configurationsUpdated()) {
            return true; //nothing to be done
        }

        //during mocpp_deinitialize(), key owners are destructed. Don't store if this container is affected
        for (auto& config : configurations) {
            if (!config->getKey()) {
                MO_DBG_DEBUG("don't write back container with destructed key(s)");
                return false;
            }
        }

        size_t jsonCapacity = 2 * JSON_OBJECT_SIZE(2); //head + configurations + head payload
        jsonCapacity += JSON_ARRAY_SIZE(configurations.size()); //configurations array
        jsonCapacity += configurations.size() * JSON_OBJECT_SIZE(3); //config entries in array

        if (jsonCapacity > MO_MAX_JSON_CAPACITY) {
            MO_DBG_ERR("configs JSON exceeds maximum capacity (%s, %zu entries). Crop configs file (by FCFS)", getFilename(), configurations.size());
            jsonCapacity = MO_MAX_JSON_CAPACITY;
        }

        DynamicJsonDocument doc {jsonCapacity};
        JsonObject head = doc.createNestedObject("head");
        head["content-type"] = "ocpp_config_file";
        head["version"] = "2.0";

        JsonArray configurationsArray = doc.createNestedArray("configurations");

        size_t trackCapacity = 0;

        for (size_t i = 0; i < configurations.size(); i++) {
            auto& config = *configurations[i];

            size_t entryCapacity = JSON_OBJECT_SIZE(3) + (JSON_ARRAY_SIZE(2) - JSON_ARRAY_SIZE(1));
            if (trackCapacity + entryCapacity > MO_MAX_JSON_CAPACITY) {
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
                    stored["value"] = config.getString();
                    break;
            }
        }

        bool success = FilesystemUtils::storeJson(filesystem, getFilename(), doc);

        if (success) {
            MO_DBG_DEBUG("Saving configurations finished");
        } else {
            MO_DBG_ERR("could not save configs file: %s", getFilename());
        }

        return success;
    }

    std::shared_ptr<Configuration> createConfiguration(TConfig type, const char *key) override {
        auto res = std::shared_ptr<Configuration>(makeConfiguration(type, key).release(), std::default_delete<Configuration>(), Allocator<Configuration>(key));
        if (!res) {
            //allocation failure - OOM
            MO_DBG_ERR("OOM");
            return nullptr;
        }
        configurations.push_back(res);
        return res;
    }

    void remove(Configuration *config) override {
        const char *key = config->getKey();
        configurations.erase(std::remove_if(configurations.begin(), configurations.end(),
            [config] (std::shared_ptr<Configuration>& entry) {
                return entry.get() == config;
            }), configurations.end());
        if (key) {
            clearKeyPool(key);
        }
    }

    size_t size() override {
        return configurations.size();
    }

    Configuration *getConfiguration(size_t i) override {
        return configurations[i].get();
    }

    std::shared_ptr<Configuration> getConfiguration(const char *key) override {
        for (auto& entry : configurations) {
            if (entry->getKey() && !strcmp(entry->getKey(), key)) {
                return entry;
            }
        }
        return nullptr;
    }

    void loadStaticKey(Configuration& config, const char *key) override {
        config.setKey(key);
        clearKeyPool(key);
    }
};

std::unique_ptr<ConfigurationContainer> makeConfigurationContainerFlash(std::shared_ptr<FilesystemAdapter> filesystem, const char *filename, bool accessible) {
    return std::unique_ptr<ConfigurationContainer>(new ConfigurationContainerFlash(filesystem, filename, accessible));
}

} //end namespace MicroOcpp
