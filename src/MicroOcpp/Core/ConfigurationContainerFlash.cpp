// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Core/ConfigurationContainerFlash.h>
#include <ArduinoOcpp/Core/FilesystemUtils.h>
#include <ArduinoOcpp/Debug.h>

#include <algorithm>

#define MAX_CONFIGURATIONS 50

namespace ArduinoOcpp {

bool ConfigurationContainerFlash::load() {

    if (!filesystem) {
        return false;
    }

    if (configurations.size() > 0) {
        AO_DBG_ERR("Error: declared configurations before calling container->load(). " \
                    "All previously declared values won't be written back");
        (void)0;
    }

    size_t file_size = 0;
    if (filesystem->stat(getFilename(), &file_size) != 0 // file does not exist
            || file_size == 0) {                         // file exists, but empty
        AO_DBG_DEBUG("Populate FS: create configuration file");
        return save();
    }

    auto doc = FilesystemUtils::loadJson(filesystem, getFilename());
    if (!doc) {
        AO_DBG_ERR("failed to load %s", getFilename());
        return false;
    }

    JsonObject root = doc->as<JsonObject>();

    JsonObject configHeader = root["head"];

    if (strcmp(configHeader["content-type"] | "Invalid", "ao_configuration_file")) {
        AO_DBG_ERR("Unable to initialize: unrecognized configuration file format");
        return false;
    }

    if (strcmp(configHeader["version"] | "Invalid", "1.1")) {
        AO_DBG_ERR("Unable to initialize: unsupported version");
        return false;
    }
    
    JsonArray configurationsArray = root["configurations"];
    if (configurationsArray.size() > MAX_CONFIGURATIONS) {
        AO_DBG_ERR("Unable to initialize: configurations_len is too big (=%zu)", configurationsArray.size());
        return false;
    }

    for (JsonObject config : configurationsArray) {
        const char *key = config["key"] | "";
        if (!*key || !config.containsKey("value")) {
            AO_DBG_ERR("corrupt config");
            continue;
        }

        const char *type = config["type"] | "Undefined";

        std::shared_ptr<AbstractConfiguration> configuration = nullptr;

        if (!strcmp(type, SerializedType<int>::get()) && config["value"].is<int>()){
            configuration = std::make_shared<Configuration<int>>(key, config["value"].as<int>());
        } else if (!strcmp(type, SerializedType<float>::get()) && config["value"].is<float>()){
            configuration = std::make_shared<Configuration<float>>(key, config["value"].as<float>());
        } else if (!strcmp(type, SerializedType<bool>::get()) && config["value"].is<bool>()){
            configuration = std::make_shared<Configuration<bool>>(key, config["value"].as<bool>());
        } else if (!strcmp(type, SerializedType<const char *>::get()) && config["value"].is<const char*>()){
            configuration = std::make_shared<Configuration<const char *>>(key, config["value"].as<const char*>());
        } else {
            AO_DBG_ERR("corrupt config");
            continue;
        }

        if (configuration) {
            configurations.push_back(configuration);
        } else {
            AO_DBG_ERR("Initialization fault: could not read key-value pair %s of type %s", config["key"].as<const char *>(), config["type"].as<const char *>());
        }
    }

    configurationsUpdated();

    AO_DBG_DEBUG("Initialization finished");
    return true;
}

bool ConfigurationContainerFlash::save() {

    if (!filesystem) {
        return false;
    }

    if (!configurationsUpdated()) {
        return true; //nothing to be done
    }

    size_t jsonCapacity = 2 * JSON_OBJECT_SIZE(2); //head + configurations + head payload

    std::vector<std::unique_ptr<DynamicJsonDocument>> entries;

    for (auto config = configurations.begin(); config != configurations.end(); config++) {
        auto entry = (*config)->toJsonStorageEntry();
        if (entry) {
            size_t capacity = entry->memoryUsage(); //entry payload size

            if (jsonCapacity + capacity + JSON_ARRAY_SIZE(entries.size() + 1)  > AO_MAX_JSON_CAPACITY) {
                AO_DBG_ERR("configs JSON exceeds maximum capacity (%s, %zu entries). Crop configs file (by FCFS)", getFilename(), entries.size());
                break;
            }

            entries.push_back(std::move(entry));
            jsonCapacity += capacity;
        }
    }

    jsonCapacity += JSON_ARRAY_SIZE(entries.size());

    DynamicJsonDocument doc {jsonCapacity};
    JsonObject head = doc.createNestedObject("head");
    head["content-type"] = "ao_configuration_file";
    head["version"] = "1.1";

    JsonArray configurationsArray = doc.createNestedArray("configurations");
    for (auto entry = entries.begin(); entry != entries.end(); entry++) {
        configurationsArray.add((*entry)->as<JsonObject>());
    }

    bool success = FilesystemUtils::storeJson(filesystem, getFilename(), doc);

    if (success) {
        AO_DBG_DEBUG("Saving configurations finished");
    } else {
        AO_DBG_ERR("could not save configs file: %s", getFilename());
    }

    return success;
}

} //end namespace ArduinoOcpp
