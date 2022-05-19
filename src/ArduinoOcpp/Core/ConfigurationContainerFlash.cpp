// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Core/ConfigurationContainerFlash.h>
#include <ArduinoOcpp/Debug.h>

#if defined(ESP32) && !defined(AO_DEACTIVATE_FLASH)
#include <LITTLEFS.h>
#define USE_FS LITTLEFS
#else
#include <FS.h>
#define USE_FS SPIFFS
#endif

#define MAX_FILE_SIZE 4000
#define MAX_CONFIGURATIONS 50

namespace ArduinoOcpp {

bool ConfigurationContainerFlash::load() {
#ifndef AO_DEACTIVATE_FLASH

    if (configurations.size() > 0) {
        AO_DBG_ERR("Error: declared configurations before calling container->load(). " \
                    "All previously declared values won't be written back");
    }

    if (!USE_FS.exists(getFilename())) {
        AO_DBG_DEBUG("Populate FS: create configuration file");
        return true;
    }

    File file = USE_FS.open(getFilename(), "r");

    if (!file) {
        AO_DBG_ERR("Unable to initialize: could not open configuration file %s", getFilename());
        return false;
    }

    if (!file.available()) {
        AO_DBG_DEBUG("Populate FS: create configuration file");
        file.close();
        return true;
    }

    int file_size = file.size();

    if (file_size < 2) {
        AO_DBG_ERR("Unable to initialize: too short for json");
        file.close();
        return false;
    } else if (file_size > MAX_FILE_SIZE) {
        AO_DBG_ERR("Unable to initialize: filesize is too long");
        file.close();
        return false;
    }

    String token = file.readStringUntil('\n');
    if (!token.equals("content-type:arduino-ocpp_configuration_file")) {
        AO_DBG_ERR("Unable to initialize: unrecognized configuration file format");
        file.close();
        return false;
    }

    token = file.readStringUntil('\n');
    if (!token.equals("version:1.0")) {
        AO_DBG_ERR("Unable to initialize: unsupported version");
        file.close();
        return false;
    }

    token = file.readStringUntil(':');
    if (!token.equals("configurations_len")) {
        AO_DBG_ERR("Unable to initialize: missing length statement");
        file.close();
        return false;
    }

    token = file.readStringUntil('\n');
    int configurations_len = token.toInt();
    if (configurations_len <= 0) {
        AO_DBG_ERR("Unable to initialize: empty configuration");
        file.close();
        return true;
    }
    if (configurations_len > MAX_CONFIGURATIONS) {
        AO_DBG_ERR("Unable to initialize: configurations_len is too big");
        file.close();
        return false;
    }

    size_t jsonCapacity = file_size + JSON_OBJECT_SIZE(1) + JSON_ARRAY_SIZE(configurations_len) + configurations_len * JSON_OBJECT_SIZE(5);

    AO_DBG_DEBUG("Config capacity = %zu", jsonCapacity);

    DynamicJsonDocument configDoc(jsonCapacity);

    DeserializationError error = deserializeJson(configDoc, file);
    if (error) {
        AO_DBG_ERR("Unable to initialize: config file deserialization failed: %s", error.c_str());
        file.close();
        return false;
    }

    JsonArray configurationsArray = configDoc["configurations"];
    for (JsonObject config : configurationsArray) {
        const char *type = config["type"] | "Undefined";

        std::shared_ptr<AbstractConfiguration> configuration = nullptr;

        if (!strcmp(type, SerializedType<int>::get())){
            configuration = std::make_shared<Configuration<int>>(config);
        } else if (!strcmp(type, SerializedType<float>::get())){
            configuration = std::make_shared<Configuration<float>>(config);
        } else if (!strcmp(type, SerializedType<const char *>::get())){
            configuration = std::make_shared<Configuration<const char *>>(config);
        }

        if (configuration) {
            configurations.push_back(configuration);
        } else {
            AO_DBG_ERR("Initialization fault: could not read key-value pair %s of type %s", config["key"].as<const char *>(), config["type"].as<const char *>());
        }
    }

    file.close();

    configurationsUpdated();

    AO_DBG_DEBUG("Initialization successful");
#endif //ndef AO_DEACTIVATE_FLASH
    return true;
}

bool ConfigurationContainerFlash::save() {
#ifndef AO_DEACTIVATE_FLASH

    if (!configurationsUpdated()) {
        return true; //nothing to be done
    }

    if (USE_FS.exists(getFilename())) {
        USE_FS.remove(getFilename());
    }

    File file = USE_FS.open(getFilename(), "w");

    if (!file) {
        AO_DBG_ERR("Unable to save: could not open configuration file %s", getFilename());
        return false;
    }

    size_t jsonCapacity = JSON_OBJECT_SIZE(1); //configurations

    size_t numEntries = configurations.size();

    file.print("content-type:arduino-ocpp_configuration_file\n");
    file.print("version:1.0\n");
    file.print("configurations_len:");
    file.print(numEntries, DEC);
    file.print("\n");

    std::vector<std::shared_ptr<DynamicJsonDocument>> entries;

    for (auto config = configurations.begin(); config != configurations.end(); config++) {
        std::shared_ptr<DynamicJsonDocument> entry = (*config)->toJsonStorageEntry();
        if (entry)
            entries.push_back(entry);
    }

    jsonCapacity += JSON_ARRAY_SIZE(entries.size()); //length of configurations
    for (auto entry = entries.begin(); entry != entries.end(); entry++) {
        jsonCapacity += (*entry)->capacity();
    }

    DynamicJsonDocument configDoc(jsonCapacity);

    JsonArray configurationsArray = configDoc.createNestedArray("configurations");

    for (auto entry = entries.begin(); entry != entries.end(); entry++) {
        configurationsArray.add((*entry)->as<JsonObject>());
    }

    // Serialize JSON to file
    if (serializeJson(configDoc, file) == 0) {
        AO_DBG_ERR("Unable to save: Could not serialize JSON");
        file.close();
        return false;
    }

    //success
    file.close();
    AO_DBG_DEBUG("Saving configDoc successful");

#endif //ndef AO_DEACTIVATE_FLASH
    return true;
}

} //end namespace ArduinoOcpp
