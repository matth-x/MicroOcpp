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

AbstractConfiguration::AbstractConfiguration(const char *key) {
    if (!key || !*key) {
        AO_DBG_ERR("invalid argument");
        return;
    }

    this->key = key;
}

AbstractConfiguration::~AbstractConfiguration() {

}

const char *AbstractConfiguration::getKey() {
    return key.c_str();
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

void AbstractConfiguration::requireRebootWhenChanged() {
    rebootRequiredWhenChanged = true;
}

bool AbstractConfiguration::requiresRebootWhenChanged() {
    return rebootRequiredWhenChanged;
}

uint16_t AbstractConfiguration::getValueRevision() {
    return value_revision;
}

template<class T>
Configuration<T>::Configuration(const char *key, T value) : AbstractConfiguration(key) {
    this->operator=(value);
}

template <class T>
const T &Configuration<T>::operator=(const T & newVal) {

    if (permissionLocalClientCanWrite() || !initializedValue) {
        if (!initializedValue) {
            const size_t VALUE_MAXSIZE = 50;
            char value_str [VALUE_MAXSIZE] = {'\0'};
            toCStringValue(value_str, VALUE_MAXSIZE, newVal);
            AO_DBG_DEBUG("add config: key = %s, value = %s", getKey(), value_str);
        }
        if (initializedValue == true && value != newVal) {
            value_revision++;
        }
        value = newVal;
        initializedValue = true;
    } else {
        AO_DBG_ERR("Tried to override read-only configuration: %s", getKey());
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

Configuration<const char *>::Configuration(const char *key, const char *value) : AbstractConfiguration(key) {
    if (!value) {
        AO_DBG_ERR("invalid args");
        return;
    }

    this->operator=(value);
}

Configuration<const char *>::~Configuration() {

}

const char *Configuration<const char *>::operator=(const char *new_value) {
    if (!new_value) {
        AO_DBG_ERR("invalid args");
        return new_value;
    }

    if (!permissionLocalClientCanWrite() && initializedValue) {
        AO_DBG_ERR("Tried to override read-only configuration: %s", getKey());
        return new_value;
    }

    if (value.compare(new_value) || !initializedValue) {
        value = new_value;
        value_revision++;
    }
    
    if (!initializedValue) {
        AO_DBG_DEBUG("add config: key = %s, value = %s", getKey(), value.c_str());
        (void)0;
    }
    initializedValue = true;
    return new_value;
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
