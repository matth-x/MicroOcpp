// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Core/ConfigurationKeyValue.h>

#include <Variants.h>

#include <string.h>
#include <vector>
#include <ArduinoJson.h>

#if defined(ESP32) && !defined(AO_DEACTIVATE_FLASH)
#include <LITTLEFS.h>
#define USE_FS LITTLEFS
#else
#include <FS.h>
#define USE_FS SPIFFS
#endif

#define KEY_MAXLEN 60
#define STRING_VAL_MAXLEN 2000 //allow TLS certificates in ...

namespace ArduinoOcpp {

AbstractConfiguration::AbstractConfiguration() {

}

AbstractConfiguration::AbstractConfiguration(JsonObject &storedKeyValuePair) {
    if (storedKeyValuePair["key"].as<JsonVariant>().is<const char*>()) {
        setKey(storedKeyValuePair["key"]);
    } else {
        Serial.print(F("[AbstractConfiguration] Type mismatch: cannot construct with given storedKeyValuePair\n"));
    }
}

AbstractConfiguration::~AbstractConfiguration() {
    if (key != nullptr) {
        free(key);
    }
    key = nullptr;
}

void AbstractConfiguration::printKey() {
    if (key != nullptr) {
        Serial.print(key);
    }
}

size_t AbstractConfiguration::getStorageHeaderJsonCapacity() {
    return JSON_OBJECT_SIZE(1) //key
            + key_size + 1; //TODO key_size already considers 0-terminator. Is "+1" really necessary here?
}

void AbstractConfiguration::storeStorageHeader(JsonObject &keyValuePair) {
    if (toBeRemovedFlag) return;
    keyValuePair["key"] = key;
}

size_t AbstractConfiguration::getOcppMsgHeaderJsonCapacity() {
    return JSON_OBJECT_SIZE(2) //key + readonly value
            + key_size + 1; //TODO key_size already considers 0-terminator. Is "+1" really necessary here?
}

void AbstractConfiguration::storeOcppMsgHeader(JsonObject &keyValuePair) {
    if (toBeRemovedFlag) return;
    keyValuePair["key"] = key;
    if (remotePeerCanWrite) {
        keyValuePair["readonly"] = "false";
    } else {
        keyValuePair["readonly"] = "true";
    }
}

bool AbstractConfiguration::isValid() {
    return initializedValue && key != nullptr && key_size > 0 && !toBeRemovedFlag;
}

bool AbstractConfiguration::setKey(const char *newKey) {
    if (key != nullptr || key_size > 0) {
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
    value_revision++;
}

void AbstractConfiguration::resetToBeRemovedFlag() {
    toBeRemovedFlag = false;
    value_revision++;
}

uint16_t AbstractConfiguration::getValueRevision() {
    return value_revision;
}

bool AbstractConfiguration::keyEquals(const char *other) {
    return !strcmp(key, other);
}

template <class T>
Configuration<T>::Configuration() {

}

Configuration<const char *>::Configuration() {

}

template<class T>
Configuration<T>::Configuration(JsonObject &storedKeyValuePair) : AbstractConfiguration(storedKeyValuePair) {
    auto jsonEntry = storedKeyValuePair["value"].as<JsonVariant>();
    if (jsonEntry.is<T>()) {
        this->operator=(jsonEntry.as<T>());
    } else {
        Serial.print(F("[Configuration<T>] Type mismatch: cannot deserialize Json to given Type T\n"));
    }
}

template <class T>
const T &Configuration<T>::operator=(const T & newVal) {

    if (permissionLocalClientCanWrite() || !initializedValue) {
        if (DEBUG_OUT && !initializedValue) {
            Serial.print(F("[Configuration<T>] Initialized config object. Key = "));
            printKey();
            Serial.print(F(", value = "));
            Serial.println(newVal);
        }
        initializedValue = true;
        if (value != newVal) {
            value_revision++;
        }
        value = newVal;
        resetToBeRemovedFlag();
    } else {
        Serial.print(F("[Configuration] Tried to override read-only configuration: "));
        printKey();
        Serial.println();
    }
    return newVal;
}

template <class T>
Configuration<T>::operator T() {
    if (!initializedValue) {
//        Serial.print(F("[Configuration<T>] Tried to access value without preceeding initialization: "));
//        printKey();
//        Serial.println();
    }
    return value;
}

template<class T>
bool Configuration<T>::isValid() {
    return AbstractConfiguration::isValid();
}

template<class T>
size_t Configuration<T>::getValueJsonCapacity() {
    return JSON_OBJECT_SIZE(1);
}

//template<>
//size_t Configuration<String>::getValueJsonCapacity() {
//    return JSON_OBJECT_SIZE(1) + value.length() + 1;
//}

size_t Configuration<const char *>::getValueJsonCapacity() {
    return JSON_OBJECT_SIZE(1) + value_size;
}

template<class T>
std::shared_ptr<DynamicJsonDocument> Configuration<T>::toJsonStorageEntry() {
    if (!isValid() || toBeRemoved()) {
        return nullptr;
    }
    size_t capacity = getStorageHeaderJsonCapacity()
                + getValueJsonCapacity()
                + JSON_OBJECT_SIZE(3); //type, header, value
    
    std::shared_ptr<DynamicJsonDocument> doc = std::make_shared<DynamicJsonDocument>(capacity);
    JsonObject keyValuePair = doc->to<JsonObject>();
    keyValuePair["type"] = SerializedType<T>::get();
    storeStorageHeader(keyValuePair);
    keyValuePair["value"] = value;
    return doc;
}

template<class T>
std::shared_ptr<DynamicJsonDocument> Configuration<T>::toJsonOcppMsgEntry() {
    if (!isValid() || toBeRemoved()) {
        return nullptr;
    }
    size_t capacity = getOcppMsgHeaderJsonCapacity()
                + getValueJsonCapacity()
                + JSON_OBJECT_SIZE(2); // header, value
    
    std::shared_ptr<DynamicJsonDocument> doc = std::make_shared<DynamicJsonDocument>(capacity);
    JsonObject keyValuePair = doc->to<JsonObject>();
    storeOcppMsgHeader(keyValuePair);
    keyValuePair["value"] = value;
    return doc;
}

std::shared_ptr<DynamicJsonDocument> Configuration<const char *>::toJsonStorageEntry() {
    if (!isValid() || toBeRemoved()) {
        return nullptr;
    }
    size_t capacity = getStorageHeaderJsonCapacity()
                + getValueJsonCapacity()
                + JSON_OBJECT_SIZE(3); //type, header, value
    
    std::shared_ptr<DynamicJsonDocument> doc = std::make_shared<DynamicJsonDocument>(capacity);
    JsonObject keyValuePair = doc->to<JsonObject>();
    keyValuePair["type"] = SerializedType<const char *>::get();
    storeStorageHeader(keyValuePair);
    keyValuePair["value"] = value;
    return doc;
}

std::shared_ptr<DynamicJsonDocument> Configuration<const char *>::toJsonOcppMsgEntry() {
    if (!isValid() || toBeRemoved()) {
        return nullptr;
    }
    size_t capacity = getOcppMsgHeaderJsonCapacity()
                + getValueJsonCapacity()
                + JSON_OBJECT_SIZE(2); // header, value
    
    std::shared_ptr<DynamicJsonDocument> doc = std::make_shared<DynamicJsonDocument>(capacity);
    JsonObject keyValuePair = doc->to<JsonObject>();
    storeOcppMsgHeader(keyValuePair);
    keyValuePair["value"] = value;
    return doc;
}

Configuration<const char *>::Configuration(JsonObject &storedKeyValuePair) : AbstractConfiguration(storedKeyValuePair) {
    if (storedKeyValuePair["value"].as<JsonVariant>().is<const char*>()) {
        const char *storedValue = storedKeyValuePair["value"].as<JsonVariant>().as<const char*>();
        if (storedValue) {
            size_t storedValueSize = strlen(storedValue) + 1;
            storedValueSize++;
            setValue(storedValue, storedValueSize);
        } else {
            Serial.print(F("[Configuration<const char *>] Stored value is empty\n"));
        }
    } else {
        Serial.print(F("[Configuration<const char *>] Type mismatch: cannot deserialize Json to given Type T\n"));
    }
}

Configuration<const char *>::~Configuration() {
    if (value != nullptr) {
        free(value);
        value = nullptr;
    }
    if (valueReadOnlyCopy != nullptr) {
        free(valueReadOnlyCopy);
        valueReadOnlyCopy = nullptr;
    }
}

bool Configuration<const char *>::setValue(const char *new_value, size_t buffsize) {
    if (!permissionLocalClientCanWrite() && initializedValue) {
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
    
    if (value_changed || !initializedValue) {

        if (value != nullptr) {
            free(value);
            value = nullptr;
        }

        if (valueReadOnlyCopy != nullptr) {
            free(valueReadOnlyCopy);
            valueReadOnlyCopy = nullptr;
        }

        value_size = checkedBuffsize;

        value = (char *) malloc (sizeof(char) * value_size);
        valueReadOnlyCopy = (char *) malloc (sizeof(char) * value_size);
        if (!value || !valueReadOnlyCopy) {
            Serial.print(F("[Configuration<const char *>] in setValue(): Could not allocate value or value copy\n"));

            if (value != nullptr) {
                free(value);
                value = nullptr;
            }

            if (valueReadOnlyCopy != nullptr) {
                free(valueReadOnlyCopy);
                valueReadOnlyCopy = nullptr;
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
    
    if (DEBUG_OUT && !initializedValue) {
        Serial.print(F("[Configuration<const char*>] Initialized config object. Key = "));
        printKey();
        Serial.print(F(", value = "));
        Serial.println(value);
    }
    initializedValue = true;
    resetToBeRemovedFlag();
    return true;
}

const char *Configuration<const char *>::operator=(const char *newVal) {
    if (!setValue(newVal, strlen(newVal) + 1)) {
        Serial.print(F("[Configuration<const char *>] Setting value in operator= was unsuccessful!\n"));
    }
    return newVal;
}

Configuration<const char *>::operator const char*() {
    strncpy(valueReadOnlyCopy, value, value_size);
    return valueReadOnlyCopy;
}

bool Configuration<const char *>::isValid() {
    return AbstractConfiguration::isValid() && value != nullptr && valueReadOnlyCopy != nullptr && value_size > 0;
}

size_t Configuration<const char *>::getBuffsize() {
    return value_size;
}

template class Configuration<int>;
template class Configuration<float>;
template class Configuration<const char *>;

} //end namespace ArduinoOcpp
