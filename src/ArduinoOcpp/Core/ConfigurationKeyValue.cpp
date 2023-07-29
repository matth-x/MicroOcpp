// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Core/ConfigurationKeyValue.h>
#include <ArduinoOcpp/Debug.h>

#include <string.h>
#include <vector>
#include <ArduinoJson.h>

#define KEY_MAXLEN 60
#define STRING_VAL_MAXLEN 2000 //allow TLS certificates in ...

namespace ArduinoOcpp {

AbstractConfiguration::AbstractConfiguration() {

}

AbstractConfiguration::AbstractConfiguration(JsonObject &storedKeyValuePair) {
    if (!setKey(storedKeyValuePair["key"] | "")) {
        AO_DBG_ERR("validation error");
    }
}

AbstractConfiguration::~AbstractConfiguration() {

}

void AbstractConfiguration::printKey() {
    AO_CONSOLE_PRINTF("%s", key.c_str());
    (void)0;
}

size_t AbstractConfiguration::getStorageHeaderJsonCapacity() {
    return key.length() + 1;
}

void AbstractConfiguration::storeStorageHeader(JsonObject &keyValuePair) {
    keyValuePair["key"] = key;
}

size_t AbstractConfiguration::getOcppMsgHeaderJsonCapacity() {
    return key.size() + 1;
}

void AbstractConfiguration::storeOcppMsgHeader(JsonObject &keyValuePair) {
    keyValuePair["key"] = key;
    if (remotePeerCanWrite) {
        keyValuePair["readonly"] = false;
    } else {
        keyValuePair["readonly"] = true;
    }
}

bool AbstractConfiguration::isValid() {
    return initializedValue && !key.empty();
}

bool AbstractConfiguration::setKey(const char *newKey) {
    if (!key.empty()) {
        AO_DBG_ERR("cannot override key");
        return false;
    }

    if (!newKey || *newKey == '\0') {
        AO_DBG_ERR("invalid argument");
        return false;
    }

    key = newKey;
    return true;
}

void AbstractConfiguration::requireRebootWhenChanged() {
    rebootRequiredWhenChanged = true;
}

bool AbstractConfiguration::requiresRebootWhenChanged() {
    return rebootRequiredWhenChanged;
}

uint16_t AbstractConfiguration::getValueRevision() {
    return value_revision;
}

bool AbstractConfiguration::keyEquals(const char *other) {
    return !key.compare(other);
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
        AO_DBG_ERR("Type mismatch: cannot deserialize Json to given type");
    }
}

//helper functions
void printValue(int value) {
    AO_CONSOLE_PRINTF("%i", value);
}

void printValue(float value) {
    AO_CONSOLE_PRINTF("%f", value);
}

void printValue(bool value) {
    AO_CONSOLE_PRINTF("%s", value ? "true" : "false");
}

template <class T>
const T &Configuration<T>::operator=(const T & newVal) {

    if (permissionLocalClientCanWrite() || !initializedValue) {
        if (AO_DBG_LEVEL >= AO_DL_DEBUG && !initializedValue) {
            AO_DBG_DEBUG("Initialized config object:");
            AO_CONSOLE_PRINTF("[AO]     > Key = ");
            printKey();
            AO_CONSOLE_PRINTF(", value = ");
            printValue(newVal);
            AO_CONSOLE_PRINTF("\n");
        }
        if (initializedValue == true && value != newVal) {
            value_revision++;
        }
        value = newVal;
        initializedValue = true;
    } else {
        AO_DBG_ERR("Tried to override read-only configuration:");
        AO_CONSOLE_PRINTF("[AO]     > Key = ");
        printKey();
        AO_CONSOLE_PRINTF("\n");
    }
    return newVal;
}

template <class T>
Configuration<T>::operator T() {
    return value;
}

template<class T>
bool Configuration<T>::isValid() {
    return AbstractConfiguration::isValid();
}

template<class T>
size_t Configuration<T>::getValueJsonCapacity() {
    return 0;
}

size_t Configuration<const char *>::getValueJsonCapacity() {
    return value.size() + 1;
}

template<class T>
std::unique_ptr<DynamicJsonDocument> Configuration<T>::toJsonStorageEntry() {
    if (!isValid()) {
        return nullptr;
    }
    size_t capacity = getStorageHeaderJsonCapacity()
                + getValueJsonCapacity()
                + JSON_OBJECT_SIZE(3); //type, header, value
    
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(capacity));
    JsonObject keyValuePair = doc->to<JsonObject>();
    keyValuePair["type"] = SerializedType<T>::get();
    storeStorageHeader(keyValuePair);
    keyValuePair["value"] = value;
    return doc;
}

int toCStringValue(char *buf, size_t length, int value) {
    return snprintf(buf, length, "%d", value);
}

int toCStringValue(char *buf, size_t length, float value) {
    int ilength = (int) std::min((size_t) 100, length);
    return snprintf(buf, length, "%.*g", ilength >= 7 ? ilength - 7 : 0, value);
}

int toCStringValue(char *buf, size_t length, bool value) {
    return snprintf(buf, length, "%s", value ? "true" : "false");
}

template<class T>
std::unique_ptr<DynamicJsonDocument> Configuration<T>::toJsonOcppMsgEntry() {
    if (!isValid()) {
        return nullptr;
    }
    size_t capacity = getOcppMsgHeaderJsonCapacity()
                + JSON_OBJECT_SIZE(3); //header (key + readonly), value

    const size_t VALUE_MAXSIZE = 50;
    char value_str [VALUE_MAXSIZE] = {'\0'};
    toCStringValue(value_str, VALUE_MAXSIZE, value);
    capacity += strlen(value_str) + 1;
    
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(capacity));
    JsonObject keyValuePair = doc->to<JsonObject>();
    storeOcppMsgHeader(keyValuePair);
    keyValuePair["value"] = value_str;
    return doc;
}

std::unique_ptr<DynamicJsonDocument> Configuration<const char *>::toJsonStorageEntry() {
    if (!isValid()) {
        return nullptr;
    }
    size_t capacity = getStorageHeaderJsonCapacity()
                + getValueJsonCapacity()
                + JSON_OBJECT_SIZE(3); //type, header, value
    
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(capacity));
    JsonObject keyValuePair = doc->to<JsonObject>();
    keyValuePair["type"] = SerializedType<const char *>::get();
    storeStorageHeader(keyValuePair);
    keyValuePair["value"] = value;
    return doc;
}

std::unique_ptr<DynamicJsonDocument> Configuration<const char *>::toJsonOcppMsgEntry() {
    if (!isValid()) {
        return nullptr;
    }
    size_t capacity = getOcppMsgHeaderJsonCapacity()
                + getValueJsonCapacity()
                + JSON_OBJECT_SIZE(3); //header (key + readonly), value
    
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(capacity));
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
            AO_DBG_WARN("Stored value is empty");
        }
    } else {
        AO_DBG_ERR("Type mismatch: cannot deserialize Json to given type");
    }
}

Configuration<const char *>::~Configuration() {

}

bool Configuration<const char *>::setValue(const char *new_value, size_t buffsize) {
    if (!permissionLocalClientCanWrite() && initializedValue) {
        AO_DBG_ERR("Tried to override read-only configuration:");
        AO_CONSOLE_PRINTF("[AO]     > Key = ");
        return false;
    }

    if (!new_value) {
        AO_DBG_ERR("Argument is null. No change");
        return false;
    }

    if (value.compare(new_value) || !initializedValue) {
        value = new_value;
        value_revision++;
    }
    
    if (AO_DBG_LEVEL >= AO_DL_DEBUG && !initializedValue) {
        AO_DBG_DEBUG("Initialized config object:");
        AO_CONSOLE_PRINTF("[AO]     > Key = ");
        printKey();
        AO_CONSOLE_PRINTF(", value = %s\n", value.c_str());
    }
    initializedValue = true;
    return true;
}

const char *Configuration<const char *>::operator=(const char *newVal) {
    if (!setValue(newVal, strlen(newVal) + 1)) {
        AO_DBG_ERR("Setting value in operator= was unsuccessful");
    }
    return newVal;
}

Configuration<const char *>::operator const char*() {
    return value.c_str();
}

bool Configuration<const char *>::isValid() {
    return AbstractConfiguration::isValid();
}

size_t Configuration<const char *>::getBuffsize() {
    return value.size() + 1;
}

void Configuration<const char *>::setValidator(std::function<bool(const char*)> validator) {
    this->validator = validator;
}

std::function<bool(const char*)> Configuration<const char *>::getValidator() {
    return this->validator;
}

template class Configuration<int>;
template class Configuration<float>;
template class Configuration<bool>;
template class Configuration<const char *>;

} //end namespace ArduinoOcpp
