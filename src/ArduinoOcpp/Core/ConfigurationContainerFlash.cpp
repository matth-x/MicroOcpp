// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/Core/ConfigurationContainerFlash.h>
#include <Variants.h>

#if defined(ESP32)
#define USE_FS LITTLEFS
#else
#define USE_FS SPIFFS
#endif

#if USE_FS == LITTLEFS
#include <LITTLEFS.h>
#elif USE_FS == SPIFFS
#include <FS.h>
#else
#error "FS not supported"
#endif

#define MAX_FILE_SIZE 4000
#define MAX_CONFIGURATIONS 50

namespace ArduinoOcpp {

bool ConfigurationContainerFlash::load() {
#ifndef AO_DEACTIVATE_FLASH

    if (configurations.size() > 0) {
        Serial.print(F("[ConfigurationsContainerFlash] Error: declared configurations before calling container->load().\n" \
                       "                               All previously declared values won't be written back\n"));
    }

    File file = USE_FS.open(getFilename(), "r");

    if (!file) {
        Serial.print(F("[Configuration] Unable to initialize: could not open configuration file\n"));
        return false;
    }

    if (!file.available()) {
        Serial.print(F("[Configuration] Unable to initialize: empty file\n"));
        file.close();
        return false;
    }

    int file_size = file.size();

    if (file_size < 2) {
        Serial.print(F("[Configuration] Unable to initialize: too short for json\n"));
        file.close();
        return false;
    } else if (file_size > MAX_FILE_SIZE) {
        Serial.print(F("[Configuration] Unable to initialize: filesize is too long!\n"));
        file.close();
        return false;
    }

    String token = file.readStringUntil('\n');
    if (!token.equals("content-type:arduino-ocpp_configuration_file")) {
        Serial.print(F("[Configuration] Unable to initialize: unrecognized configuration file format\n"));
        file.close();
        return false;
    }

    token = file.readStringUntil('\n');
    if (!token.equals("version:1.0")) {
        Serial.print(F("[Configuration] Unable to initialize: unsupported version\n"));
        file.close();
        return false;
    }

    token = file.readStringUntil(':');
    if (!token.equals("configurations_len")) {
        Serial.print(F("[Configuration] Unable to initialize: missing length statement\n"));
        file.close();
        return false;
    }

    token = file.readStringUntil('\n');
    int configurations_len = token.toInt();
    if (configurations_len <= 0) {
        Serial.print(F("[Configuration] Initialization: Empty configuration\n"));
        file.close();
        return true;
    }
    if (configurations_len > MAX_CONFIGURATIONS) {
        Serial.print(F("[Configuration] Unable to initialize: configurations_len is too big!\n"));
        file.close();
        return false;
    }

    size_t jsonCapacity = file_size + JSON_OBJECT_SIZE(1) + JSON_ARRAY_SIZE(configurations_len) + configurations_len * JSON_OBJECT_SIZE(5);

    if (DEBUG_OUT) Serial.print(F("[Configuration] Config capacity = "));
    if (DEBUG_OUT) Serial.print(jsonCapacity);
    if (DEBUG_OUT) Serial.print(F("\n"));

    DynamicJsonDocument configDoc(jsonCapacity);

    DeserializationError error = deserializeJson(configDoc, file);
    if (error) {
        Serial.println(F("[Configuration] Unable to initialize: config file deserialization failed: "));
        Serial.print(error.c_str());
        Serial.print(F("\n"));
        file.close();
        return false;
    }

    JsonArray configurationsArray = configDoc["configurations"];
    for (JsonObject config : configurationsArray) {
        const char *type = config["type"] | "Undefined";

        std::shared_ptr<AbstractConfiguration> configuration = NULL;

        if (!strcmp(type, SerializedType<int>::get())){
            configuration = std::make_shared<Configuration<int>>(config);
        } else if (!strcmp(type, SerializedType<float>::get())){
            configuration = std::make_shared<Configuration<float>>(config);
        } else if (!strcmp(type, SerializedType<const char *>::get())){
            configuration = std::make_shared<Configuration<const char *>>(config);
//        } else if (!strcmp(type, SerializedType<String>::get())){
//            configuration = std::make_shared<Configuration<String>>(config);
        }

        if (configuration) {
            configurations.push_back(configuration);
        } else {
            Serial.print(F("[Configuration] Initialization fault: could not read key-value pair "));
            Serial.print(config["key"].as<const char *>());
            Serial.print(F(" of type "));
            Serial.print(config["type"].as<const char *>());
            Serial.print(F("\n"));
        }
    }

    file.close();

    configurationsUpdated();

    if (DEBUG_OUT) Serial.println(F("[Configuration] Initialization successful\n"));
#endif //ndef AO_DEACTIVATE_FLASH
    return true;
}

bool ConfigurationContainerFlash::save() {
#ifndef AO_DEACTIVATE_FLASH

    if (!configurationsUpdated()) {
        return true; //nothing to be done
    }

    USE_FS.remove(getFilename());

    File file = USE_FS.open(getFilename(), "w");

    if (!file) {
        Serial.print(F("[Configuration] Unable to save: could not open configuration file\n"));
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
        Serial.println(F("[Configuration] Unable to save: Could not serialize JSON\n"));
        file.close();
        return false;
    }

    //success
    file.close();
    Serial.print(F("[Configuration] Saving configDoc successful\n"));

#endif //ndef AO_DEACTIVATE_FLASH
    return true;
}

} //end namespace ArduinoOcpp
