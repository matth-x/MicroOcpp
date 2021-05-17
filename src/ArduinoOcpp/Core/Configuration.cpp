// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/Core/Configuration.h>

#include <Variants.h>

#include <string.h>
#include <LinkedList.h>
#include <ArduinoJson.h>

#if defined(ESP32) && !defined(AO_DEACTIVATE_FLASH)
#include <LITTLEFS.h>
#define USE_FS LITTLEFS
#else
#include <FS.h>
#define USE_FS SPIFFS
#endif

#define MAX_FILE_SIZE 4000
#define MAX_CONFIGURATIONS 50

#define KEY_MAXLEN 60
#define STRING_VAL_MAXLEN 500

#define CONFIGURATION_FN "/arduino-ocpp.cnf"

#define DEV_CONF true

namespace ArduinoOcpp {

AbstractConfiguration::AbstractConfiguration() {

}

AbstractConfiguration::AbstractConfiguration(JsonObject storedKeyValuePair) {
    if (storedKeyValuePair["key"].as<JsonVariant>().is<const char*>()) {
        setKey(storedKeyValuePair["key"]);
    } else {
        Serial.print(F("[AbstractConfiguration] Type mismatch: cannot construct with given storedKeyValuePair\n"));
    }
}

AbstractConfiguration::~AbstractConfiguration() {
    if (key != NULL) {
        free(key);
    }
    key = NULL;
}

void AbstractConfiguration::printKey() {
    if (key != NULL) {
        Serial.print(key);
    }
}

size_t AbstractConfiguration::getStorageHeaderJsonCapacity() {
    return JSON_OBJECT_SIZE(1) 
            + key_size + 1; //TODO key_size already considers 0-terminator. Is "+1" really necessary here?
}

void AbstractConfiguration::storeStorageHeader(JsonObject keyValuePair) {
    if (toBeRemovedFlag) return;
    keyValuePair["key"] = key;
}

size_t AbstractConfiguration::getOcppMsgHeaderJsonCapacity() {
    return JSON_OBJECT_SIZE(2) 
            + key_size + 1; //TODO key_size already considers 0-terminator. Is "+1" really necessary here?
}

void AbstractConfiguration::storeOcppMsgHeader(JsonObject keyValuePair) {
    if (toBeRemovedFlag) return;
    keyValuePair["key"] = key;
    if (writepermission) {
        keyValuePair["readonly"] = "false";
    } else {
        keyValuePair["readonly"] = "true";
    }
}

bool AbstractConfiguration::isValid() {
    return initializedValue && key != NULL && key_size > 0 && !toBeRemovedFlag;
}

bool AbstractConfiguration::setKey(const char *newKey) {
    if (key != NULL || key_size > 0) {
        Serial.print(F("[AbstractConfiguration] Cannot change key or set key twice! Keep old value\n"));
        return false;
    }

    key_size = strlen(newKey) + 1; //plus 0-terminator

    if (key_size > KEY_MAXLEN + 1) {
        Serial.print(F("[AbstractConfiguration] in setKey(): Maximal key length exceeded: "));
        Serial.print(newKey);
        Serial.print(F(". Abort\n"));
        return false;
    } else if (key_size <= 1) {
        Serial.print(F("[AbstractConfiguration] in setKey(): Null or empty key not allowed! Abort\n"));
        return false;
    }

    key = (char *) malloc(sizeof(char) * key_size);
    if (!key) {
        Serial.print(F("[AbstractConfiguration] in setKey(): Could not allocate key\n"));
        return false;
    }

    strcpy(key, newKey);

    return true;
}

void AbstractConfiguration::revokeWritePermission() {
    writepermission = false;
}

bool AbstractConfiguration::hasWritePermission() {
    return writepermission;
}

void AbstractConfiguration::requireRebootWhenChanged() {
    rebootRequiredWhenChanged = true;
}

bool AbstractConfiguration::requiresRebootWhenChanged() {
    return rebootRequiredWhenChanged;
}

bool AbstractConfiguration::toBeRemoved() {
    return toBeRemovedFlag;
}

void AbstractConfiguration::setToBeRemoved() {
    toBeRemovedFlag = true;
}

void AbstractConfiguration::resetToBeRemovedFlag() {
    toBeRemovedFlag = false;
}

uint16_t AbstractConfiguration::getValueRevision() {
    return value_revision;
}

int AbstractConfiguration::compareKey(const char *other) {
    return strcmp(key, other);
}


template <class T>
Configuration<T>::Configuration() {

}

template <class T>
Configuration<T>::Configuration(JsonObject storedKeyValuePair) : AbstractConfiguration(storedKeyValuePair) {
    if (storedKeyValuePair["value"].as<JsonVariant>().is<T>()) {
        initializeValue(storedKeyValuePair["value"].as<JsonVariant>().as<T>());
    } else {
        Serial.print(F("[Configuration<T>] Type mismatch: cannot deserialize Json to given Type T\n"));
    }
}

template <class T>
T Configuration<T>::operator=(T newVal) {

    if (hasWritePermission()) {
        if (value != newVal) {
            dirty = true;
            value_revision++;
        }
        value = newVal;
        initializedValue = true;
        resetToBeRemovedFlag();
    } else {
        Serial.print(F("[Configuration] Tried to override read-only configuration: "));
        printKey();
        Serial.println();
    }
    return value;
}

//template<class T>
//Configuration<T>::operator T() {
//    if (!initializedValue) {
//        Serial.print(F("[Configuration<T>] Tried to access value without preceeding initialization: "));
//        printKey();
//        Serial.println();
//    }
//    return value;
//}

template<class T>
bool Configuration<T>::isValid() {
    return AbstractConfiguration::isValid();
}

//template<class T>
//Configuration<T>::operator bool() {
//    return isValid();
//}

template<class T>
void Configuration<T>::initializeValue(T init) {
    if (initializedValue) {
        Serial.print(F("[Configuration<T>] Tried to initialize value twice for: "));
        printKey();
        Serial.println();
        return;
    }

    *this = init;

    if (DEBUG_OUT) Serial.print(F("[Configuration<T>] Initialized config object. Key = "));
    if (DEBUG_OUT) printKey();
    if (DEBUG_OUT) Serial.print(F(", value = "));
    if (DEBUG_OUT) Serial.println(value);
}

template<class T>
std::shared_ptr<DynamicJsonDocument> Configuration<T>::toJsonStorageEntry() {
    if (!isValid() || toBeRemoved()) {
        return NULL;
    }
    size_t capacity = getStorageHeaderJsonCapacity()
                + JSON_OBJECT_SIZE(3); //type, header, value
    
    std::shared_ptr<DynamicJsonDocument> doc = std::make_shared<DynamicJsonDocument>(capacity);
    JsonObject keyValuePair = doc->to<JsonObject>();
    keyValuePair["type"] = getSerializedTypeSpecifier();
    storeStorageHeader(keyValuePair);
    keyValuePair["value"] = value;
    return doc;
}

//template<>
//std::shared_ptr<DynamicJsonDocument> Configuration<float>::toJsonStorageEntry() {
//    if (!isValid() || toBeRemoved()) {
//        return NULL;
//    }
//
//    /*
//     * need to enforce at least one decimal place
//     */
//    String serialized = String(value, 6); //6 decimal places, i.e. value = 42.1234 --> serialized = 42.123400
//    unsigned int trunicated_len = serialized.length(); //without trailing 0
//    char *trunicated_value = (char *) malloc(sizeof(char) * (trunicated_len + 1)); //malloc includes trailing 0
//    serialized.toCharArray(trunicated_value, trunicated_len + 1);
//    while (trunicated_len > 1) {
//        unsigned int index_last = trunicated_len - 1;
//        if (trunicated_value[index_last] == '0' && trunicated_value[index_last - 1] != '.') {
//            trunicated_value[index_last] = '\0';
//            trunicated_len--;
//        } else {
//            break;
//        }
//    }
//
//    size_t capacity = getStorageHeaderJsonCapacity()
//                + JSON_OBJECT_SIZE(1)
//                + trunicated_len + 1; //value
//    
//    std::shared_ptr<DynamicJsonDocument> doc = std::make_shared<DynamicJsonDocument>(capacity);
//    JsonObject keyValuePair = doc->to<JsonObject>();
//    storeStorageHeader(keyValuePair);
//    keyValuePair["value"] = trunicated_value;
//    free(trunicated_value);
//    return doc;
//}

template<>
const char *Configuration<int>::getSerializedTypeSpecifier() {
    return "int";
}

template<>
const char *Configuration<float>::getSerializedTypeSpecifier() {
    return "float";
}

template<class T>
std::shared_ptr<DynamicJsonDocument> Configuration<T>::toJsonOcppMsgEntry() {
    if (!isValid() || toBeRemoved()) {
        return NULL;
    }
    size_t capacity = getOcppMsgHeaderJsonCapacity()
                + JSON_OBJECT_SIZE(1); // value
    
    std::shared_ptr<DynamicJsonDocument> doc = std::make_shared<DynamicJsonDocument>(capacity);
    JsonObject keyValuePair = doc->to<JsonObject>();
    storeOcppMsgHeader(keyValuePair);
    keyValuePair["value"] = value;
    return doc;
}

Configuration<const char *>::Configuration() {

}

Configuration<const char *>::Configuration(JsonObject storedKeyValuePair) : AbstractConfiguration(storedKeyValuePair) {
    if (storedKeyValuePair["value"].as<JsonVariant>().is<const char*>()) {
        const char *storedValue = storedKeyValuePair["value"].as<JsonVariant>().as<const char*>();
        if (storedValue) {
            size_t storedValueSize = strlen(storedValue) + 1;
            storedValueSize++;
            initializeValue(storedValue, storedValueSize);
        } else {
            Serial.print(F("[Configuration<const char *>] Stored value is empty\n"));
        }
    } else {
        Serial.print(F("[Configuration<const char *>] Type mismatch: cannot deserialize Json to given Type T\n"));
    }
}

Configuration<const char *>::~Configuration() {
    if (value != NULL) {
        free(value);
        value = NULL;
    }
    if (valueReadOnlyCopy != NULL) {
        free(valueReadOnlyCopy);
        valueReadOnlyCopy = NULL;
    }
}

bool Configuration<const char *>::setValue(const char *new_value, size_t buffsize) {

    if (!hasWritePermission()) {
        Serial.print(F("[Configuration<const char *>] Tried to override read-only configuration: "));
        printKey();
        Serial.println();
        return false;
    }

    if (!new_value) {
        Serial.print(F("[Configuration<const char *>] in setValue(): Argument is null! No change\n"));
        return false;
    }

    //"ab"
    //buffsize=5
    //a != 0 -> checkedBuffsize = 1
    //b != 0 -> checkedBuffsize = 2
    //0 == 0 -> checkedBuffsize = 3
    //break
    size_t checkedBuffsize = 0;
    for (size_t i = 0; i < buffsize; i++) {
        if (new_value[i] != '\0') {
            checkedBuffsize++;
        } else {
            checkedBuffsize++;
            break;
        }
    }

    if (checkedBuffsize <= 0) {
        Serial.print(F("[Configuration<const char *>] in setValue(): buffsize is <= 0! No change\n"));
        return false;
    }

    if (checkedBuffsize > STRING_VAL_MAXLEN + 1) {
        Serial.print(F("[Configuration<const char *>] in setValue(): Maximal value length exceeded! Abort\n"));
        return false;
    }

    bool value_changed = true;
    if (checkedBuffsize == value_size) {
        value_changed = false;
        for (size_t i = 0; i < value_size; i++) {
            if (value[i] != new_value[i]) {
                value_changed = true;
                break;
            }
        }

    }
    
    if (value_changed) {

        if (value != NULL) {
            free(value);
            value = NULL;
        }

        if (valueReadOnlyCopy != NULL) {
            free(valueReadOnlyCopy);
            valueReadOnlyCopy = NULL;
        }

        value_size = checkedBuffsize;

        value = (char *) malloc (sizeof(char) * value_size);
        valueReadOnlyCopy = (char *) malloc (sizeof(char) * value_size);
        if (!value || !valueReadOnlyCopy) {
            Serial.print(F("[Configuration<const char *>] in setValue(): Could not allocate value or value copy\n"));

            if (value != NULL) {
                free(value);
                value = NULL;
            }

            if (valueReadOnlyCopy != NULL) {
                free(valueReadOnlyCopy);
                valueReadOnlyCopy = NULL;
            }

            value_size = 0;
            return false;
        }

        strncpy(value, new_value, value_size);
        strncpy(valueReadOnlyCopy, new_value, value_size);

        value[value_size-1] = '\0';
        valueReadOnlyCopy[value_size-1] = '\0';

        value_revision++;
    }
    
    resetToBeRemovedFlag();
    initializedValue = true;
    return true;
}

String &Configuration<const char *>::operator=(String &newVal) {
    if (!setValue(newVal.c_str(), newVal.length() + 1)) {
        Serial.print(F("[Configuration<const char *>] Setting value in operator= was unsuccessful!\n"));
    }
    return newVal;
}

Configuration<const char *>::operator const char*() {
    strncpy(valueReadOnlyCopy, value, value_size);
    return valueReadOnlyCopy;
}

//Configuration<const char *>::operator String() {
//    return String(value);
//}

bool Configuration<const char *>::isValid() {
    return AbstractConfiguration::isValid() && value != NULL && valueReadOnlyCopy != NULL && value_size > 0;
}

//Configuration<const char *>::operator bool() {
//    return isValid();
//}

size_t Configuration<const char *>::getBuffsize() {
    return value_size;
}

bool Configuration<const char *>::initializeValue(const char *initval, size_t initsize) {
    if (initializedValue) {
        Serial.print(F("[Configuration<const char *>] Tried to initialize value twice for: "));
        printKey();
        Serial.println();
        return false;
    }

    if (DEBUG_OUT) Serial.print(F("[Configuration<const char*>] Initializing config object. Key = "));
    if (DEBUG_OUT) printKey();
    if (DEBUG_OUT) Serial.print(F(", value = "));
    if (DEBUG_OUT) Serial.println(initval);

    return setValue(initval, initsize);
}

std::shared_ptr<DynamicJsonDocument> Configuration<const char *>::toJsonStorageEntry() {
    if (!isValid() || toBeRemoved()) {
        return NULL;
    }

    size_t capacity = getStorageHeaderJsonCapacity()
                + JSON_OBJECT_SIZE(3) //type, header, value
                + value_size + 1; //TODO value_size already considers 0-terminator. Is "+1" really necessary here?

    std::shared_ptr<DynamicJsonDocument> doc = std::make_shared<DynamicJsonDocument>(capacity);
    JsonObject keyValuePair = doc->to<JsonObject>();
    keyValuePair["type"] = getSerializedTypeSpecifier();
    storeStorageHeader(keyValuePair);
    keyValuePair["value"] = value;
    return doc;
}

std::shared_ptr<DynamicJsonDocument> Configuration<const char *>::toJsonOcppMsgEntry() {
    if (!isValid() || toBeRemoved()) {
        return NULL;
    }

    size_t capacity = getOcppMsgHeaderJsonCapacity()
                + JSON_OBJECT_SIZE(2) //header, value
                + value_size + 1; //TODO value_size already considers 0-terminator. Is "+1" really necessary here?

    std::shared_ptr<DynamicJsonDocument> doc = std::make_shared<DynamicJsonDocument>(capacity);
    JsonObject keyValuePair = doc->to<JsonObject>();
    storeOcppMsgHeader(keyValuePair);
    keyValuePair["value"] = value;
    return doc;
}

//LinkedList<AbstractConfiguration> configuration_all = LinkedList<AbstractConfiguration>();

LinkedList<AbstractConfiguration*> abc = LinkedList<AbstractConfiguration*>();

LinkedList<std::shared_ptr<Configuration<int>>> configuration_ints = LinkedList<std::shared_ptr<Configuration<int>>>();
LinkedList<std::shared_ptr<Configuration<float>>> configuration_floats = LinkedList<std::shared_ptr<Configuration<float>>>();
LinkedList<std::shared_ptr<Configuration<const char*>>> configuration_strings = LinkedList<std::shared_ptr<Configuration<const char*>>>();

template<class T>
std::shared_ptr<Configuration<T>> getConfiguration(LinkedList<std::shared_ptr<Configuration<T>>> *collection, const char *key) {

    for (int i = 0; i < collection->size(); i++) {
        if (!collection->get(i)->compareKey(key)) {
            return collection->get(i);
        }
    }
    return NULL;
}

template<class T>
std::shared_ptr<Configuration<T>> createConfiguration(const char *key, T value) {

    std::shared_ptr<Configuration<T>> configuration = std::make_shared<Configuration<T>>();
        
    if (!configuration->setKey(key)) {
        Serial.print(F("[Configuration] In createConfiguration(key, T, ...) : Cannot set key! Abort\n"));
        return NULL;
    }

    configuration->initializeValue(value);

    return configuration;
}

std::shared_ptr<Configuration<const char*>> createConfiguration(const char *key, const char *value, size_t buffsize) {

    std::shared_ptr<Configuration<const char*>> configuration = std::make_shared<Configuration<const char*>>();
        
    if (!configuration->setKey(key)) {
        Serial.print(F("[Configuration] In createConfiguration(key, const char*, ...) : Cannot set key! Abort\n"));
        return NULL;
    }

    configuration->initializeValue(value, buffsize);

    return configuration;
}

std::shared_ptr<AbstractConfiguration> removeConfiguration(const char *key) {

    std::shared_ptr<AbstractConfiguration> configuration;

    configuration = getConfiguration(&configuration_ints, key);
    if (configuration) {
        if (configuration->hasWritePermission())
            configuration->setToBeRemoved();
            return configuration;
    }
    
    configuration = getConfiguration(&configuration_floats, key);
    if (configuration) {
        if (configuration->hasWritePermission())
            configuration->setToBeRemoved();
            return configuration;
    }
    
    configuration = getConfiguration(&configuration_strings, key);
    if (configuration) {
        if (configuration->hasWritePermission())
            configuration->setToBeRemoved();
            return configuration;
    }
    
    return NULL;
}

std::shared_ptr<Configuration<int>> declareConfiguration(const char *key, int defaultValue, bool writePermission, bool rebootRequiredWhenChanged) {
    //already existent? --> stored in last session --> do set default content, but set writepermission flag

    std::shared_ptr<Configuration<int>> configuration = getConfiguration(&configuration_ints, key);

    if (!configuration) {
        configuration = createConfiguration(key, defaultValue);
        if (!configuration) {
            Serial.print(F("[Configuration] In declareConfiguration(key, int, ...) : Cannot find configuration stored from previous session and cannot create new one! Abort\n"));
            return NULL;
        }
        configuration_ints.add(configuration);
    } else if (configuration->toBeRemoved()) { //corner case
        *configuration = defaultValue;
    }

    if (!writePermission)
        configuration->revokeWritePermission();
    
    if (rebootRequiredWhenChanged)
        configuration->requireRebootWhenChanged();
    
    return configuration;
}

std::shared_ptr<Configuration<float>> declareConfiguration(const char *key, float defaultValue, bool writePermission, bool rebootRequiredWhenChanged) {
    //already existent? --> stored in last session --> do set default content, but set writepermission flag

    std::shared_ptr<Configuration<float>> configuration = getConfiguration(&configuration_floats, key);

    if (!configuration) {
        configuration = createConfiguration(key, defaultValue);
        if (!configuration) {
            Serial.print(F("[Configuration] In declareConfiguration(key, float, ...) : Cannot find configuration stored from previous session and cannot create new one! Abort\n"));
            return NULL;
        }
        configuration_floats.add(configuration);
    } else if (configuration->toBeRemoved()) { //corner case
        *configuration = defaultValue;
    }

    if (!writePermission)
        configuration->revokeWritePermission();
    
    if (rebootRequiredWhenChanged)
        configuration->requireRebootWhenChanged();
    
    return configuration;
}

std::shared_ptr<Configuration<const char *>> declareConfiguration(const char *key, const char *defaultValue, bool writePermission, bool rebootRequiredWhenChanged) {
    //already existent? --> stored in last session --> do set default content, but set writepermission flag

    std::shared_ptr<Configuration<const char *>> configuration = getConfiguration(&configuration_strings, key);

    if (!configuration) {
        configuration = createConfiguration(key, defaultValue, strlen(defaultValue) + 1);

        if (!configuration) {
            Serial.print(F("[Configuration] In declareConfiguration(key, const char *, ...) : Cannot find configuration stored from previous session and cannot create new one! Abort\n"));
            return NULL;
        }

        configuration_strings.add(configuration);
    } else if (configuration->toBeRemoved()) { //corner case
        configuration->setValue(defaultValue, strlen(defaultValue) + 1);
    }

    if (!writePermission)
        configuration->revokeWritePermission();
    
    if (rebootRequiredWhenChanged)
        configuration->requireRebootWhenChanged();
    
    return configuration;
}

std::shared_ptr<Configuration<int>> setConfiguration(const char *key, int value) {

    std::shared_ptr<Configuration<int>> configuration = getConfiguration(&configuration_ints, key);

    if (configuration) {
        *configuration = value;
    } else {
        configuration = createConfiguration(key, value);
        if (!configuration) {
            Serial.print(F("[Configuration] In setConfiguration(key, int) : Cannot neither find configuration nor create new one! Abort\n"));
            return NULL;
        }
        configuration_ints.add(configuration);
    }
    
    return configuration;
}

std::shared_ptr<Configuration<float>> setConfiguration(const char *key, float value) {

    std::shared_ptr<Configuration<float>> configuration = getConfiguration(&configuration_floats, key);

    if (configuration) {
        *configuration = value;
    } else {
        configuration = createConfiguration(key, value);
        if (!configuration) {
            Serial.print(F("[Configuration] In setConfiguration(key, float) : Cannot neither find configuration nor create new one! Abort\n"));
            return NULL;
        }
        configuration_floats.add(configuration);
    }
    
    return configuration;
}

std::shared_ptr<Configuration<const char *>> setConfiguration(const char *key, const char *value, size_t buffsize) {

    std::shared_ptr<Configuration<const char *>> configuration = getConfiguration(&configuration_strings, key);

    if (configuration) {
        configuration->setValue(value, buffsize);
    } else {
        configuration = createConfiguration(key, value, buffsize);
        if (!configuration) {
            Serial.print(F("[Configuration] In setConfiguration(key, const char* value, buffsize) : Cannot neither find configuration nor create new one! Abort\n"));
            return NULL;
        }
        configuration_strings.add(configuration);
    }
    
    return configuration;
}

std::shared_ptr<Configuration<int>> getConfiguration_Int(const char *key) {
    return getConfiguration(&configuration_ints, key);
}

std::shared_ptr<Configuration<float>> getConfiguration_Float(const char *key) {
    return getConfiguration(&configuration_floats, key);
}

std::shared_ptr<Configuration<const char*>> getConfiguration_string(const char *key) {
    return getConfiguration(&configuration_strings, key);
}

std::shared_ptr<AbstractConfiguration> getConfiguration(const char *key) { //TODO maybe use iterator like "getAllConfigurations"?
    std::shared_ptr<AbstractConfiguration> result = NULL;

    result = getConfiguration_Int(key);
    if (result)
        return result;

    result = getConfiguration_Float(key);
    if (result)
        return result;

    result = getConfiguration_string(key);
    if (result)
        return result;

    return NULL;
}

std::shared_ptr<LinkedList<std::shared_ptr<AbstractConfiguration>>> getAllConfigurations() { //TODO maybe change to iterator?
    auto result = std::make_shared<LinkedList<std::shared_ptr<AbstractConfiguration>>>();

    for (int i = 0; i < configuration_ints.size(); i++)
        result->add(configuration_ints.get(i));

    for (int i = 0; i < configuration_floats.size(); i++)
        result->add(configuration_floats.get(i));
    
    for (int i = 0; i < configuration_strings.size(); i++)
        result->add(configuration_strings.get(i));

    return result;
}

bool configuration_init(bool formatOnFail) {
#ifndef AO_DEACTIVATE_FLASH

#if defined(ESP32)
    if(!LITTLEFS.begin(formatOnFail)) {
        Serial.println("[Configuration] An Error has occurred while mounting LITTLEFS");
        return false;
    }
#else
    //ESP8266
    SPIFFSConfig cfg;
    cfg.setAutoFormat(formatOnFail);
    SPIFFS.setConfig(cfg);

    if (!SPIFFS.begin()) {
        Serial.print(F("[Configuration] Unable to initialize: unable to mount FS\n"));
        return false;
    }
#endif

    File file = USE_FS.open(CONFIGURATION_FN, "r");

    if (!file) {
        Serial.print(F("[Configuration] Unable to initialize: could not open configuration file\n"));
        return false;
    }

#if 0
    Serial.print(F("[Configuration] DEBUG: dump configuration file contents: \n"));
    while (file.available()) {
        Serial.write(file.read());
    }
    Serial.print(F("[Configuration] DEBUG: end configuration file\n"));
    file.close();
    file = SPIFFS.open(CONFIGURATION_FN, "r");

    if (!file) {
        Serial.print(F("[Configuration] Unable to initialize: could not open configuration file\n"));
        return false;
    }
#endif

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
    if (!token.equals("content-type:esp8266-ocpp_configuration_file")) {
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

    size_t jsonCapacity = file_size + JSON_OBJECT_SIZE(1) + JSON_ARRAY_SIZE(configurations_len) + configurations_len * JSON_OBJECT_SIZE(3);

    if (DEBUG_OUT) Serial.print(F("[Configuration] Config capacity = "));
    if (DEBUG_OUT) Serial.print(jsonCapacity);
    if (DEBUG_OUT) Serial.print(F("\n"));

    DynamicJsonDocument config(jsonCapacity);

    DeserializationError error = deserializeJson(config, file);
    if (error) {
        Serial.println(F("[Configuration] Unable to initialize: config file deserialization failed: "));
        Serial.print(error.c_str());
        Serial.print(F("\n"));
        file.close();
        return false;
    }

    JsonArray configurations = config["configurations"];
    for (int i = 0; i < configurations.size(); i++) {
        JsonObject pair = configurations[i];
        const char *type = pair["type"] | "Undefined";

        //backwards compatibility. Will be removed in a few months
        if (!strcmp(type, "Undefined")) {
            if (pair["value"].as<JsonVariant>().is<int>()){
                type = "int";
            } else if (pair["value"].as<JsonVariant>().is<float>()){
                type = "float";
            } else if (pair["value"].as<JsonVariant>().is<const char*>()){
                type = "string";
            }
        } //end backwards compatibility

        if (!strcmp(type, "int")){
            std::shared_ptr<Configuration<int>> configuration = std::make_shared<Configuration<int>>(pair);
            configuration_ints.add(configuration);
        } else if (!strcmp(type, "float")){
            std::shared_ptr<Configuration<float>> configuration = std::make_shared<Configuration<float>>(pair);
            configuration_floats.add(configuration);
        } else if (!strcmp(type, "string")){
            std::shared_ptr<Configuration<const char*>> configuration = std::make_shared<Configuration<const char*>>(pair);
            configuration_strings.add(configuration);
        } else {
            Serial.print(F("[Configuration] Initialization fault: could not read key-value pair "));
            Serial.print(pair["key"].as<const char *>());
            Serial.print(F("\n"));
        }
    }

    file.close();

    Serial.println(F("[Configuration] Initialization successful\n"));
#endif //ndef AO_DEACTIVATE_FLASH
    return true;
}

bool configuration_save() {
#ifndef AO_DEACTIVATE_FLASH

    USE_FS.remove(CONFIGURATION_FN);

    File file = USE_FS.open(CONFIGURATION_FN, "w");

    if (!file) {
        Serial.print(F("[Configuration] Unable to save: could not open configuration file\n"));
        return false;
    }

    size_t jsonCapacity = JSON_OBJECT_SIZE(1); //configurations

    std::shared_ptr<LinkedList<std::shared_ptr<AbstractConfiguration>>> allConfigurations = getAllConfigurations();

    int numEntries = allConfigurations->size();

    file.print("content-type:esp8266-ocpp_configuration_file\n");
    file.print("version:1.0\n");
    file.print("configurations_len:");
    file.print(numEntries, DEC);
    file.print("\n");

    LinkedList<std::shared_ptr<DynamicJsonDocument>> entries = LinkedList<std::shared_ptr<DynamicJsonDocument>>();

    for (int i = 0; i < allConfigurations->size(); i++) {
        std::shared_ptr<DynamicJsonDocument> entry = allConfigurations->get(i)->toJsonStorageEntry();
        if (entry)
            entries.add(entry);
    }

    jsonCapacity += JSON_ARRAY_SIZE(entries.size()); //length of configurations
    for (int i = 0; i < entries.size(); i++) {
        jsonCapacity += entries.get(i)->capacity();
    }

    DynamicJsonDocument config(jsonCapacity);

    JsonArray configurations = config.createNestedArray("configurations");

    for (int i = 0; i < entries.size(); i++) {
        configurations.add(entries.get(i)->as<JsonObject>());
    }

    // Serialize JSON to file
    if (serializeJson(config, file) == 0) {
        Serial.println(F("[Configuration] Unable to save: Could not serialize JSON\n"));
        file.close();
        return false;
    }

    //success
    file.close();
    Serial.print(F("[Configuration] Saving config successful\n"));

#endif //ndef AO_DEACTIVATE_FLASH
    return true;
}

} //end namespace ArduinoOcpp
