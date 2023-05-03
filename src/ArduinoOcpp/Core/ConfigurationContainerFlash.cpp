// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Core/ConfigurationContainerFlash.h>
#include <ArduinoOcpp/Core/FilesystemUtils.h>
#include <ArduinoOcpp/Debug.h>

#include <algorithm>

#define MAX_FILE_SIZE 4000
#define MAX_CONFIGURATIONS 50
#define MAX_CONFJSON_CAPACITY 4000

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

    if (file_size > MAX_FILE_SIZE) {
        AO_DBG_ERR("Unable to initialize: filesize is too long");
        return false;
    }

    auto file = filesystem->open(getFilename(), "r");

    if (!file) {
        AO_DBG_ERR("Unable to initialize: could not open configuration file %s", getFilename());
        return false;
    }

    auto jsonCapacity = std::max(file_size, (size_t) 256);
    DynamicJsonDocument *doc = NULL;
    DeserializationError err = DeserializationError::NoMemory;

    while (err == DeserializationError::NoMemory) {
        if (jsonCapacity > MAX_CONFJSON_CAPACITY) {
            AO_DBG_ERR("JSON capacity exceeded");
            if (doc) {
                delete(doc);
            }
            return false;
        }

        AO_DBG_DEBUG("Configs JSON capacity: %zu", jsonCapacity);

        if (doc) {
            delete(doc);
        }
        doc = new DynamicJsonDocument(jsonCapacity);
        ArduinoJsonFileAdapter file_adapt {file.get()};
        err = deserializeJson(*doc, file_adapt);

        jsonCapacity *= 3;
        jsonCapacity /= 2;
        file->seek(0);
    }

    if (err) {
        AO_DBG_ERR("Unable to initialize: config file deserialization failed: %s", err.c_str());
        delete(doc);
        return false;
    }

    JsonObject configHeader = (*doc)["head"];

    if (strcmp(configHeader["content-type"] | "Invalid", "ao_configuration_file")) {
        AO_DBG_ERR("Unable to initialize: unrecognized configuration file format");
        delete(doc);
        return false;
    }

    if (strcmp(configHeader["version"] | "Invalid", "1.1")) {
        AO_DBG_ERR("Unable to initialize: unsupported version");
        delete(doc);
        return false;
    }
    
    JsonArray configurationsArray = (*doc)["configurations"];
    if (configurationsArray.size() > MAX_CONFIGURATIONS) {
        AO_DBG_ERR("Unable to initialize: configurations_len is too big (=%zu)", configurationsArray.size());
        delete(doc);
        return false;
    }

    for (JsonObject config : configurationsArray) {
        const char *type = config["type"] | "Undefined";

        std::shared_ptr<AbstractConfiguration> configuration = nullptr;

        if (!strcmp(type, SerializedType<int>::get())){
            configuration = std::make_shared<Configuration<int>>(config);
        } else if (!strcmp(type, SerializedType<float>::get())){
            configuration = std::make_shared<Configuration<float>>(config);
        } else if (!strcmp(type, SerializedType<bool>::get())){
            configuration = std::make_shared<Configuration<bool>>(config);
        } else if (!strcmp(type, SerializedType<const char *>::get())){
            configuration = std::make_shared<Configuration<const char *>>(config);
        }

        if (configuration) {
            configurations.push_back(configuration);
        } else {
            AO_DBG_ERR("Initialization fault: could not read key-value pair %s of type %s", config["key"].as<const char *>(), config["type"].as<const char *>());
        }
    }

    configurationsUpdated();

    AO_DBG_DEBUG("Initialization finished");
    delete(doc);
    return true;
}

bool ConfigurationContainerFlash::save() {

    if (!filesystem) {
        return false;
    }

    if (!configurationsUpdated()) {
        return true; //nothing to be done
    }

    size_t file_size = 0;
    if (filesystem->stat(getFilename(), &file_size) == 0) {
        filesystem->remove(getFilename());
    }

    auto file = filesystem->open(getFilename(), "w");
    if (!file) {
        AO_DBG_ERR("Unable to save: could not open configuration file %s", getFilename());
        return false;
    }

    size_t jsonCapacity = 2 * JSON_OBJECT_SIZE(2); //head + configurations + head payload

    std::vector<std::shared_ptr<DynamicJsonDocument>> entries;

    for (auto config = configurations.begin(); config != configurations.end(); config++) {
        std::shared_ptr<DynamicJsonDocument> entry = (*config)->toJsonStorageEntry();
        if (entry) {
            entries.push_back(entry);
        }
        if (entries.size() >= MAX_CONFIGURATIONS) {
            AO_DBG_ERR("Max No of configratuions exceeded. Crop configs file (by FCFS)");
            break;
        }
    }

    jsonCapacity += JSON_ARRAY_SIZE(entries.size()); //length of configurations
    for (auto entry = entries.begin(); entry != entries.end(); entry++) {
        jsonCapacity += (*entry)->capacity();
    }

    jsonCapacity = std::max(jsonCapacity, (size_t) 256);
    DynamicJsonDocument *doc = NULL;
    bool jsonDocOverflow = true;

    while (jsonDocOverflow) {
        if (jsonCapacity > MAX_CONFJSON_CAPACITY) {
            AO_DBG_ERR("JSON capacity exceeded");
            if (doc) {
                delete(doc);
            }
            return false;
        }

        file->seek(0);

        if (doc) {
            delete(doc);
        }
        doc = new DynamicJsonDocument(jsonCapacity);
        JsonObject head = doc->createNestedObject("head");
        head["content-type"] = "ao_configuration_file";
        head["version"] = "1.1";

        JsonArray configurationsArray = doc->createNestedArray("configurations");
        for (auto entry = entries.begin(); entry != entries.end(); entry++) {
            configurationsArray.add((*entry)->as<JsonObject>());
        }

        ArduinoJsonFileAdapter file_adapt {file.get()};
        size_t written = serializeJson(*doc, file_adapt);

        jsonCapacity *= 3;
        jsonCapacity /= 2;
        jsonDocOverflow = doc->overflowed();

        if (!jsonDocOverflow && written < 20) { //plausibility check
            AO_DBG_ERR("Config serialization: unkown error for file %s", getFilename());
            delete(doc);
            return false;
        }
    }

    //success
    AO_DBG_DEBUG("Saving configurations finished");
    delete(doc);
    return true;
}

} //end namespace ArduinoOcpp
