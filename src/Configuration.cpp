// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#include "Configuration.h"

#include "Variants.h"

#include <string.h>
#include <LinkedList.h>
#include <ArduinoJson.h>

#include <FS.h>

#define MAX_FILE_SIZE 4000
#define MAX_CONFIGURATIONS 50

#define KEY_MAXLEN 60
#define STRING_VAL_MAXLEN 500

#define CONFIGURATION_FN "/esp8266-ocpp.cnf"

void changeConfigurationNotify(const char *key);

/********************************************************************************
 * DECLARATIONS
 ********************************************************************************/

class Configuration_Integer {
private:
    char *key = NULL;
    int value;
    bool defined = false;
public:
    Configuration_Integer();
    ~Configuration_Integer();
    bool setKey(const char *key);
    char *getKeyBuffer();
    bool setValue(int value);
    bool keyEquals(const char *key2);
    bool getValue(int *value);
    size_t getKeyValueJsonCapacity();
};

class Configuration_Float {
private:
    char *key = NULL;
    float value;
    bool defined = false;
public:
    Configuration_Float();
    ~Configuration_Float();
    bool setKey(const char *key);
    char *getKeyBuffer();
    bool setValue(float value);
    bool keyEquals(const char *key2);
    bool getValue(float *value);
    size_t getKeyValueJsonCapacity();
};

class Configuration_string {
private:
    char *key = NULL;
    char *value = NULL;
    size_t value_len;
public:
    Configuration_string();
    ~Configuration_string();
    bool setKey(const char *key);
    char *getKeyBuffer();
    bool setValue(const char *value);
    bool setValue(const char *value, size_t buffsize);
    bool keyEquals(const char *key2);
    size_t getValueBuffsize();
    char *getValueBuffer();
    bool getValue(char *value);
    size_t getKeyValueJsonCapacity();
};

#if 0 //future refactoring measure: seperate functions for the key from the type of the value

template <class T>
class Configuration {
private:
    char *key = NULL;
    size_t key_len = 0;
    bool writepermission = false;
    uint16_t value_revision = 0; //number of changes of member "value". This will be important for the client to detect if there was a change
    //idea: LinkedList<Observer> observers = ...; //think about it twice: Use Observer-interface or function pointers? How will observers unsubscribe?
    size_t value_len = 0;
    T value;
public:
    Configuration();
    ~Configuration();

    //idea: commit(); //probably better than global commit(): what if configurations will be split into multiple files in future?
}

#endif

/********************************************************************************
 * IMPLEMENTATION Configuration_Integer
 ********************************************************************************/

Configuration_Integer::Configuration_Integer() {

}

bool Configuration_Integer::setKey(const char *new_key) {
    if (key) {
        Serial.print(F("[Configuration_Integer] in setKey(): Key already set! No change\n"));
        return false;
    }

    size_t key_len = strlen(new_key);
    if (key_len > KEY_MAXLEN) {
        Serial.print(F("[Configuration_Integer] in setKey(): Maximal key length exceeded: "));
        Serial.print(new_key);
        Serial.print(F(". Abort\n"));
        return false;
    } else if (key_len <= 1) {
        Serial.print(F("[Configuration_Integer] in setKey(): Empty key not allowed! Abort\n"));
        return false;
    }

    key = (char *) malloc(sizeof(char) * (key_len + 1));
    if (!key) {
        Serial.print(F("[Configuration_Integer] in setKey(): Could not allocate key\n"));
        return false;
    }

    strcpy(key, new_key);

    return true; //success
}

char *Configuration_Integer::getKeyBuffer() {
    return key;
}

bool Configuration_Integer::setValue(const int new_value) {
    value = new_value;
    defined = true;
    return true;
}

Configuration_Integer::~Configuration_Integer() {
    if (key) free(key);
}

bool Configuration_Integer::keyEquals(const char *key2) {
    if (!key || !key2) {
        return false;
    }

    return strcmp(key, key2) == 0;
}

bool Configuration_Integer::getValue(int *out) {
    if (!defined) return false;
    *out = value;
    return true;
}

size_t Configuration_Integer::getKeyValueJsonCapacity() {
    if (!key || !defined) return 0;

    return strlen(key) + 1; //only the key adds extra space in the json doc
}

/********************************************************************************
 * IMPLEMENTATION Configuration_Float
 ********************************************************************************/

Configuration_Float::Configuration_Float() {

}

bool Configuration_Float::setKey(const char *new_key) {
    if (key) {
        Serial.print(F("[Configuration_Float] in setKey(): Key already set! No change\n"));
        return false;
    }

    size_t key_len = strlen(new_key);
    if (key_len > KEY_MAXLEN) {
        Serial.print(F("[Configuration_Float] in setKey(): Maximal key length exceeded: "));
        Serial.print(new_key);
        Serial.print(F(". Abort\n"));
        return false;
    } else if (key_len <= 1) {
        Serial.print(F("[Configuration_Float] in setKey(): Empty key not allowed! Abort\n"));
        return false;
    }

    key = (char *) malloc(sizeof(char) * (key_len + 1));
    if (!key) {
        Serial.print(F("[Configuration_Float] in setKey(): Could not allocate key\n"));
        return false;
    }

    strcpy(key, new_key);

    return true; //success
}

char *Configuration_Float::getKeyBuffer() {
    return key;
}

bool Configuration_Float::setValue(const float new_value) {
    value = new_value;
    defined = true;
    return true;
}

Configuration_Float::~Configuration_Float() {
    if (key) free(key);
}

bool Configuration_Float::keyEquals(const char *key2) {
    if (!key || !key2) {
        return false;
    }

    return strcmp(key, key2) == 0;
}

bool Configuration_Float::getValue(float *out) {
    if (!defined) return false;
    *out = value;
    return true;
}

size_t Configuration_Float::getKeyValueJsonCapacity() {
    if (!key || !defined) return 0;

    return strlen(key) + 1; //only the key adds extra space in the json doc
}

/********************************************************************************
 * IMPLEMENTATION Configuration_string
 ********************************************************************************/

Configuration_string::Configuration_string() {

}

bool Configuration_string::setKey(const char *new_key) {
    if (key) {
        Serial.print(F("[Configuration_string] in setKey(): Key already set! No change\n"));
        return false;
    }

    size_t key_len = strlen(new_key);
    if (key_len > KEY_MAXLEN) {
        Serial.print(F("[Configuration_string] in setKey(): Maximal key length exceeded: "));
        Serial.print(new_key);
        Serial.print(F(". Abort\n"));
        return false;
    } else if (key_len < 1) {
        Serial.print(F("[Configuration_string] in setKey(): Empty key not allowed! Abort\n"));
        return false;
    }

    key = (char *) malloc(sizeof(char) * (key_len + 1));
    if (!key) {
        Serial.print(F("[Configuration_string] in setKey(): Could not allocate key\n"));
        return false;
    }

    strcpy(key, new_key);

    return true; //success
}

char *Configuration_string::getKeyBuffer() {
    return key;
}

bool Configuration_string::setValue(const char *new_value) {
    if (!new_value) {
        Serial.print(F("[Configuration_string] in setValue(): Argument is null! No change\n"));
        return false;
    }

    value_len = strlen(new_value);
    if (value_len > STRING_VAL_MAXLEN) {
        Serial.print(F("[Configuration_string] in setValue(): Maximal value length exceeded! Abort\n"));
        return false;
    }

    if (value) {
        free(value);
        value = NULL;
    }

    value = (char *) malloc (sizeof(char) * (value_len + 1));
    if (!value) {
        Serial.print(F("[Configuration_string] in setValue(): Could not allocate value\n"));
        return false;
    }

    strcpy(value, new_value);
    
    return true;
}

bool Configuration_string::setValue(const char *new_value, size_t buffsize) {
    if (!new_value) {
        Serial.print(F("[Configuration_string] in setValue(): Argument is null! No change\n"));
        return false;
    }

    value_len = buffsize - 1;
    if (value_len > STRING_VAL_MAXLEN) {
        Serial.print(F("[Configuration_string] in setValue(): Maximal value length or buffsize exceeded! Abort\n"));
        return false;
    } else if (value_len < 0) {
        Serial.print(F("[Configuration_string] in setValue(): Empty value or buffize not allowed! Abort\n"));
        return false;
    }

    if (value) {
        free(value);
        value = NULL;
    }

    value = (char *) malloc (sizeof(char) * (value_len + 1));
    if (!value) {
        Serial.print(F("[Configuration_string] in setValue(): Could not allocate value\n"));
        return false;
    }

    strncpy(value, new_value, (value_len + 1));

    value[value_len] = '\0'; //ensure that this is a \0-terminated string
    
    return true;
}

Configuration_string::~Configuration_string() {
    if (key) free(key);
    if (value) free(value);
}

bool Configuration_string::keyEquals(const char *key2) {
    if (!key || !key2) {
        return false;
    }

    return strcmp(key, key2) == 0;
}

size_t Configuration_string::getValueBuffsize() {
    return value_len + 1;
}

char *Configuration_string::getValueBuffer() {
    if (value) {
        //ensure that this is a \0-terminated string to prevent faults
        value[value_len] = '\0';
    }
    return value;
}

bool Configuration_string::getValue(char *out) {
    if (!value || !out) return false;

    //ensure that this is a \0-terminated string to prevent faults
    value[value_len] = '\0';

    strcpy(out, value);

    return true;
}

size_t Configuration_string::getKeyValueJsonCapacity() {
    if (!key || !value) return 0;

    return strlen(key) + 1 + value_len + 1;
}

/********************************************************************************
 * Configuration storage logic
 ********************************************************************************/

LinkedList<Configuration_Integer*> conf_ints = LinkedList<Configuration_Integer*>();
LinkedList<Configuration_Float*> conf_floats = LinkedList<Configuration_Float*>();
LinkedList<Configuration_string*> conf_strings = LinkedList<Configuration_string*>();

bool configuration_init() {
    SPIFFSConfig cfg;
    cfg.setAutoFormat(true);
    SPIFFS.setConfig(cfg);

    if (!SPIFFS.begin()) {
        Serial.print(F("[Configuration] Unable to initialize: unable to mount FS\n"));
        return false;
    }

    File file = SPIFFS.open(CONFIGURATION_FN, "r");

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
        if (DEBUG_OUT) Serial.print(F("[Configuration] Unable to initialize: filesize is too long!\n"));
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
        if (DEBUG_OUT) Serial.print(F("[Configuration] Initialization: Empty configuration\n"));
        file.close();
        return true;
    }
    if (configurations_len > MAX_CONFIGURATIONS) {
        if (DEBUG_OUT) Serial.print(F("[Configuration] Unable to initialize: configurations_len is too big!\n"));
        file.close();
        return false;
    }

    size_t jsonCapacity = file_size + JSON_OBJECT_SIZE(1) + JSON_ARRAY_SIZE(configurations_len) + configurations_len * JSON_OBJECT_SIZE(2);

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

 //   String header = config["content-type"];
 //   String version = config["version"];
 //   if (!header.equals("esp8266-ocpp configuration file") || !version.equals("1.0")) {
 //       Serial.println(F("[Configuration] Unsupported configuration file format. Try anyway\n"));
 //   }

    JsonArray configurations = config["configurations"];
    for (int i = 0; i < configurations.size(); i++) {
        JsonObject pair = configurations[i];
        if (pair["value"].as<JsonVariant>().is<int>()){
            setConfiguration_Int(pair["key"], pair["value"]);
        } else if (pair["value"].as<JsonVariant>().is<float>()){
            setConfiguration_Float(pair["key"], pair["value"]);
        } else if (pair["value"].as<JsonVariant>().is<const char*>()){
            setConfiguration_string(pair["key"], pair["value"]);
        } else {
            Serial.print(F("[Configuration] Initialization fault: could not read key-value pair "));
            Serial.print(pair["key"].as<const char *>());
            Serial.print(F("\n"));
        }
    }

    file.close();

    Serial.println(F("[Configuration] Initialization successful\n"));
    return true;
}

bool configuration_save() {

    SPIFFS.remove(CONFIGURATION_FN);

    File file = SPIFFS.open(CONFIGURATION_FN, "w");

    if (!file) {
        Serial.print(F("[Configuration] Unable to save: could not open configuration file\n"));
        return false;
    }

//    size_t jsonCapacity = JSON_OBJECT_SIZE(3); //content-type, version, configurations
    size_t jsonCapacity = JSON_OBJECT_SIZE(1); //configurations

    int numEntries = conf_ints.size() + conf_floats.size() + conf_strings.size();

    file.print("content-type:esp8266-ocpp_configuration_file\n");
    file.print("version:1.0\n");
    file.print("configurations_len:");
    file.print(numEntries, DEC);
    file.print("\n");

    jsonCapacity += JSON_ARRAY_SIZE(numEntries); //length of configurations
    jsonCapacity += numEntries * JSON_OBJECT_SIZE(2); //each configuration is a nested object

    for (int i = 0; i < conf_ints.size(); i++) {
        jsonCapacity += conf_ints.get(i)->getKeyValueJsonCapacity();
    }
    for (int i = 0; i < conf_floats.size(); i++) {
        jsonCapacity += conf_floats.get(i)->getKeyValueJsonCapacity();
    }
    for (int i = 0; i < conf_strings.size(); i++) {
        jsonCapacity += conf_strings.get(i)->getKeyValueJsonCapacity();
    }

    DynamicJsonDocument config(jsonCapacity);

//    config["content-type"] = "esp8266-ocpp configuration file";
//    config["version"] = "1.0";
    JsonArray configurations = config.createNestedArray("configurations");

    for (int i = 0; i < conf_ints.size(); i++) {
        char *key = conf_ints.get(i)->getKeyBuffer();
        if (!key)
            continue; //not a valid entry. Don't serialize
        int value = 0;
        if (!conf_ints.get(i)->getValue(&value))
            continue; //not a valid entry. Don't serialize

        JsonObject pair = configurations.createNestedObject();
        pair["key"] = key;
        pair["value"] = value;
    }
    for (int i = 0; i < conf_floats.size(); i++) {
        char *key = conf_floats.get(i)->getKeyBuffer();
        if (!key)
            continue; //not a valid entry. Don't serialize
        float value = 0.f;
        if (!conf_floats.get(i)->getValue(&value))
            continue; //not a valid entry. Don't serialize

        JsonObject pair = configurations.createNestedObject();
        pair["key"] = key;
        pair["value"] = value;
    }
    for (int i = 0; i < conf_strings.size(); i++) {
        char *key = conf_strings.get(i)->getKeyBuffer();
        if (!key)
            continue; //not a valid entry. Don't serialize
        char *value = conf_strings.get(i)->getValueBuffer();
        if (!value)
            continue; //not a valid entry. Don't serialize

        JsonObject pair = configurations.createNestedObject();
        pair["key"] = key;
        pair["value"] = value;
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

//    // BEGIN DEBUG
//    file = SPIFFS.open(CONFIGURATION_FN, "r");
//
//    Serial.println(file.readStringUntil('\n')); //content-type
//    Serial.println(file.readStringUntil('\n')); //version
//    Serial.println(file.readStringUntil('\n')); //length
//    Serial.println(file.readStringUntil('\n')); //content
//
//    file.close();
//    // END DEBUG

    return true;
}

/********************************************************************************
 * Configuration setters
 ********************************************************************************/

bool setConfiguration_Int(const char *key, int value) {
    bool success = true;

    Configuration_Integer *entry;
    bool new_entry = true;

    for (int i = 0; i < conf_ints.size(); i++) {
        if (conf_ints.get(i)->keyEquals(key)) {
            entry = conf_ints.get(i);
            new_entry = false;
        }
    }

    if (new_entry) {
        entry = new Configuration_Integer();
        success &= entry->setKey(key);
    }

    success &= entry->setValue(value);
    
    if (new_entry) {
        if (success) {
            conf_ints.add(entry);
        } else {
            delete(entry);
        }
    }

    if (success) {
        changeConfigurationNotify(key);
    }
    
    return success;
}

bool setConfiguration_Float(const char *key, float value) {
    bool success = true;

    Configuration_Float *entry;
    bool new_entry = true;

    for (int i = 0; i < conf_floats.size(); i++) {
        if (conf_floats.get(i)->keyEquals(key)) {
            entry = conf_floats.get(i);
            new_entry = false;
        }
    }

    if (new_entry) {
        entry = new Configuration_Float();
        success &= entry->setKey(key);
    }

    success &= entry->setValue(value);
    
    if (new_entry) {
        if (success) {
            conf_floats.add(entry);
        } else {
            delete(entry);
        }
    }

    if (success) {
        changeConfigurationNotify(key);
    }
    
    return success;
}

bool setConfiguration_string(const char *key, const char *value) {
    bool success = true;

    Configuration_string *entry;
    bool new_entry = true;

    for (int i = 0; i < conf_strings.size(); i++) {
        if (conf_strings.get(i)->keyEquals(key)) {
            entry = conf_strings.get(i);
            new_entry = false;
        }
    }

    if (new_entry) {
        entry = new Configuration_string();
        success &= entry->setKey(key);
    }

    success &= entry->setValue(value);
    
    if (new_entry) {
        if (success) {
            conf_strings.add(entry);
        } else {
            delete(entry);
        }
    }

    if (success) {
        changeConfigurationNotify(key);
    }
    
    return success;
}

bool setConfiguration_string(const char *key, const char *value, size_t buffsize) {
    bool success = true;

    Configuration_string *entry;
    bool new_entry = true;

    for (int i = 0; i < conf_strings.size(); i++) {
        if (conf_strings.get(i)->keyEquals(key)) {
            entry = conf_strings.get(i);
            new_entry = false;
        }
    }

    if (new_entry) {
        entry = new Configuration_string();
        success &= entry->setKey(key);
    }

    success &= entry->setValue(value, buffsize);
    
    if (new_entry) {
        if (success) {
            conf_strings.add(entry);
        } else {
            delete(entry);
        }
    }

    if (success) {
        changeConfigurationNotify(key);
    }
    
    return success;
}

/********************************************************************************
 * Configuration default value setters
 ********************************************************************************/

bool defaultConfiguration_Int(const char *key, int value) {
    bool success = true;

    Configuration_Integer *entry;
    bool new_entry = true; //TODO replace with configurationContains_Type!

    for (int i = 0; i < conf_ints.size(); i++) {
        if (conf_ints.get(i)->keyEquals(key)) {
            entry = conf_ints.get(i);
            new_entry = false;
        }
    }

    if (new_entry) {
        success = setConfiguration_Int(key, value);
    }

    return success;
}

bool defaultConfiguration_Float(const char *key, float value) {
    bool success = true;

    Configuration_Float *entry;
    bool new_entry = true;

    for (int i = 0; i < conf_floats.size(); i++) {
        if (conf_floats.get(i)->keyEquals(key)) {
            entry = conf_floats.get(i);
            new_entry = false;
        }
    }

    if (new_entry) {
        success = setConfiguration_Float(key, value);
    }

    return success;
}

bool defaultConfiguration_string(const char *key, const char *value) {
    bool success = true;

    Configuration_string *entry;
    bool new_entry = true;

    for (int i = 0; i < conf_strings.size(); i++) {
        if (conf_strings.get(i)->keyEquals(key)) {
            entry = conf_strings.get(i);
            new_entry = false;
        }
    }

    if (new_entry) {
        success = setConfiguration_string(key, value);
    }

    return success;
}

bool defaultConfiguration_string(const char *key, const char *value, size_t buffsize) {
    bool success = true;

    Configuration_string *entry;
    bool new_entry = true;

    for (int i = 0; i < conf_strings.size(); i++) {
        if (conf_strings.get(i)->keyEquals(key)) {
            entry = conf_strings.get(i);
            new_entry = false;
        }
    }

    if (new_entry) {
        success = setConfiguration_string(key, value, buffsize);
    }

    return success;
}

/********************************************************************************
 * Configuration getters
 ********************************************************************************/

bool getConfiguration_Int(const char *key, int *value){

    Configuration_Integer *entry = NULL;
    for (int i = 0; i < conf_ints.size(); i++) {
        if (conf_ints.get(i)->keyEquals(key)) {
            entry = conf_ints.get(i);
        }
    }

    if (!entry) {
        return false;
    }

    return entry->getValue(value);
}

bool getConfiguration_Float(const char *key, float *value){

    Configuration_Float *entry = NULL;
    for (int i = 0; i < conf_floats.size(); i++) {
        if (conf_floats.get(i)->keyEquals(key)) {
            entry = conf_floats.get(i);
        }
    }

    if (!entry) {
        return false;
    }

    return entry->getValue(value);
}

bool getConfigurationBuffsize_string(const char *key, size_t *buffsize) {
    
    Configuration_string *entry = NULL;
    for (int i = 0; i < conf_strings.size(); i++) {
        if (conf_strings.get(i)->keyEquals(key)) {
            entry = conf_strings.get(i);
        }
    }

    if (!entry) {
        return false;
    }

    *buffsize = entry->getValueBuffsize();

    return true;
}

bool getConfigurationBuffer_string(const char *key, char **value) {

    Configuration_string *entry = NULL;
    for (int i = 0; i < conf_strings.size(); i++) {
        if (conf_strings.get(i)->keyEquals(key)) {
            entry = conf_strings.get(i);
        }
    }

    if (!entry) {
        return false;
    }

    *value = entry->getValueBuffer();

    return value != NULL;
}

bool getConfiguration_string(const char *key, char *value) {
    Configuration_string *entry = NULL;
    for (int i = 0; i < conf_strings.size(); i++) {
        if (conf_strings.get(i)->keyEquals(key)) {
            entry = conf_strings.get(i);
        }
    }

    if (!entry) {
        return false;
    }

    return entry->getValue(value);
}

/********************************************************************************
 * Configuration existency check
 ********************************************************************************/

bool configurationContains_Int(const char *key) {
    for (int i = 0; i < conf_ints.size(); i++) {
        if (conf_ints.get(i)->keyEquals(key)) {
            return true;
        }
    }

    return false;
}

bool configurationContains_Float(const char *key) {
    for (int i = 0; i < conf_floats.size(); i++) {
        if (conf_floats.get(i)->keyEquals(key)) {
            return true;
        }
    }

    return false;
}

bool configurationContains_string(const char *key) {
    for (int i = 0; i < conf_strings.size(); i++) {
        if (conf_strings.get(i)->keyEquals(key)) {
            return true;
        }
    }

    return false;
}

/********************************************************************************
 * Determine if EVSE should reject or require reboot
 ********************************************************************************/

// TODO encapsulate with setting the default values!

bool checkReadOnly(const char *key) {
    if (!strcmp(key, "ChargeProfileMaxStackLevel")) {
        return true;
    }
    return false;
}

bool checkRebootRequired(const char *key) {
    if (!strcmp(key, "CS_URL_PREFIX")) {
        return true;
    }
    if (!strcmp(key, "E55C_CPSERIAL")) {
        return true;
    }
    if (!strcmp(key, "E55C_METER_TYPE")) {
        return true;
    }
    return false;
}

/********************************************************************************
 * Observers
 ********************************************************************************/

LinkedList<String> changeConfigurationObserverTriggers = LinkedList<String>();
typedef void(*ChangeConfigurationObserverAction)();
LinkedList<ChangeConfigurationObserverAction> changeConfigurationObserverActions = LinkedList<ChangeConfigurationObserverAction>();

void addChangeConfigurationObserver(const char *key, void fn()) {
    changeConfigurationObserverTriggers.add(key);
    changeConfigurationObserverActions.add(fn);
}

void changeConfigurationNotify(const char *key) {
    for (int i = 0; i < changeConfigurationObserverTriggers.size(); i++) {
        if (!strcmp(key, changeConfigurationObserverTriggers.get(i).c_str())) {
            if (changeConfigurationObserverActions.get(i) != NULL)
                (changeConfigurationObserverActions.get(i))();
        }
    }
}

/********************************************************************************
 * Configuration helpers
 ********************************************************************************/

DynamicJsonDocument *serializeConfiguration(const char *key) {
    {
        Configuration_Integer *entry = NULL;
        for (int i = 0; i < conf_ints.size(); i++) {
            if (conf_ints.get(i)->keyEquals(key)) {
                entry = conf_ints.get(i);
            }
        }

        if (entry) {
            DynamicJsonDocument *result = new DynamicJsonDocument(entry->getKeyValueJsonCapacity() + JSON_OBJECT_SIZE(3));
            JsonObject configurationKey = result->to<JsonObject>();
            if (entry->getKeyBuffer()) {
                configurationKey["key"] = entry->getKeyBuffer();
            } else {
                delete result;
                return NULL;
            }
            configurationKey["readonly"] = "false";
            int value = 0;
            if (entry->getValue(&value)){
                configurationKey["value"] = value;
                return result;
            } else {
                delete result;
                return NULL;
            }
            return result;
        }
    }

    {
        Configuration_Float *entry = NULL;
        for (int i = 0; i < conf_floats.size(); i++) {
            if (conf_floats.get(i)->keyEquals(key)) {
                entry = conf_floats.get(i);
            }
        }

        if (entry) {
            DynamicJsonDocument *result = new DynamicJsonDocument(entry->getKeyValueJsonCapacity() + JSON_OBJECT_SIZE(3));
            JsonObject configurationKey = result->to<JsonObject>();
            if (entry->getKeyBuffer()) {
                configurationKey["key"] = entry->getKeyBuffer();
            } else {
                delete result;
                return NULL;
            }
            configurationKey["readonly"] = "false";
            float value = 0;
            if (entry->getValue(&value)){
                configurationKey["value"] = value;
                return result;
            } else {
                delete result;
                return NULL;
            }
            return result;
        }
    }

    {
        Configuration_string *entry = NULL;
        for (int i = 0; i < conf_strings.size(); i++) {
            if (conf_strings.get(i)->keyEquals(key)) {
                entry = conf_strings.get(i);
            }
        }

        if (entry) {
            DynamicJsonDocument *result = new DynamicJsonDocument(entry->getKeyValueJsonCapacity() + JSON_OBJECT_SIZE(3));
            JsonObject configurationKey = result->to<JsonObject>();
            if (entry->getKeyBuffer()) {
                configurationKey["key"] = entry->getKeyBuffer();
            } else {
                delete result;
                return NULL;
            }
            configurationKey["readonly"] = "false";
            if (entry->getValueBuffer()) {
                configurationKey["value"] = entry->getValueBuffer();
                return result;
            } else {
                delete result;
                return NULL;
            }
            return result;
        }
    }

    return NULL; //configuration doesn't exist or is undefined
}

DynamicJsonDocument *serializeConfiguration() {

    size_t jsonCapacity = 0;

    for (int i = 0; i < conf_ints.size(); i++) {
        jsonCapacity += conf_ints.get(i)->getKeyValueJsonCapacity();
    }

    for (int i = 0; i < conf_floats.size(); i++) {
        jsonCapacity += conf_floats.get(i)->getKeyValueJsonCapacity();
    }

    for (int i = 0; i < conf_strings.size(); i++) {
        jsonCapacity += conf_strings.get(i)->getKeyValueJsonCapacity();
    }
    
    int numEntries = conf_ints.size() + conf_floats.size() + conf_strings.size();

    jsonCapacity += JSON_ARRAY_SIZE(numEntries) + numEntries * JSON_OBJECT_SIZE(3);

    DynamicJsonDocument *result = new DynamicJsonDocument(jsonCapacity);
    JsonArray configurations = result->to<JsonArray>();

    for (int i = 0; i < conf_ints.size(); i++) {
        char *key = conf_ints.get(i)->getKeyBuffer();
        if (!key)
            continue; //not a valid entry. Don't serialize
        int value = 0;
        if (!conf_ints.get(i)->getValue(&value))
            continue; //not a valid entry. Don't serialize

        JsonObject keyValue = configurations.createNestedObject();
        keyValue["key"] = key;
        keyValue["readonly"] = "false";
        keyValue["value"] = value;
    }

    for (int i = 0; i < conf_floats.size(); i++) {
        char *key = conf_floats.get(i)->getKeyBuffer();
        if (!key)
            continue; //not a valid entry. Don't serialize
        float value = 0.f;
        if (!conf_floats.get(i)->getValue(&value))
            continue; //not a valid entry. Don't serialize

        JsonObject keyValue = configurations.createNestedObject();
        keyValue["key"] = key;
        keyValue["readonly"] = "false";
        keyValue["value"] = value;
    }

    for (int i = 0; i < conf_strings.size(); i++) {
        char *key = conf_strings.get(i)->getKeyBuffer();
        if (!key)
            continue; //not a valid entry. Don't serialize
        char *value = conf_strings.get(i)->getValueBuffer();
        if (!value)
            continue; //not a valid entry. Don't serialize

        JsonObject keyValue = configurations.createNestedObject();
        keyValue["key"] = key;
        keyValue["readonly"] = "false";
        keyValue["value"] = value;
    }

    return result;
}
