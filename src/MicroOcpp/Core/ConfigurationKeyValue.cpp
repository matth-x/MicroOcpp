// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Core/ConfigurationKeyValue.h>
#include <MicroOcpp/Debug.h>

#include <string.h>
#include <vector>
#include <ArduinoJson.h>

#define KEY_MAXLEN 60
#define STRING_VAL_MAXLEN 512

#ifndef MOCPP_CONFIG_TYPECHECK
#define MOCPP_CONFIG_TYPECHECK 1 //enable this for debugging
#endif

namespace MicroOcpp {

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



uint16_t Configuration::getValueRevision() {
    return value_revision;
}

void Configuration::setRequiresReboot() {
    rebootRequired = true;
}

bool Configuration::getRequiresReboot() {
    return rebootRequired;
}

void Configuration::revokeWritePermission() {
    writePermission = false;
}

bool Configuration::getWritePermission() {
    return writePermission;
}

void Configuration::revokeReadPermission() {
    readPermission = false;
}

bool Configuration::getReadPermission() {
    return readPermission;
}

void Configuration::setValidator(std::function<bool(const char*)> validator) {
    this->validator = validator;
}

std::function<bool(const char*)> Configuration::getValidator() {
    return validator;
}

namespace ConfigHelpers {

bool initKey(const char *dest, const char *src) {
    dest = src;
    return true;
}

bool initKey(char *dest, const char *src) {
    size_t len = strlen(src);
    dest = (char*) malloc(len + 1);
    if (!dest) {
        return false;
    }
    strcpy(dest, src);
    return true;
}

void deinitKey(const char *key) {
    (void)key;
}

void deinitKey(char *key) {
    free(key);
}

template<class T>
void deinitValue(T) { }

template<>
void deinitValue(char *val) {
    free(val);
}

template<class T>
T getNull();

template<>
int getNull() {return 0;}

template<>
float getNull() {return 0.f;}

template<>
char *getNull() {return nullptr;}

template<>
const char *getNull() {return nullptr;}

template<class T, class U>
T as(U) {return getNull<T>();}

template<>
int as(int v) {return v;}

template<>
float as(float v) {return v;}

template<>
const char *as(char *v) {return v;}

template<>
const char *as(const char *v) {return v;}


template<class T>
struct TConfigType {
    static TConfig get() {return TConfig::None;}
};

template<> struct TConfigType<int> {static TConfig get() {return TConfig::Int;}};
template<> struct TConfigType<float> {static TConfig get() {return TConfig::Float;}};
template<> struct TConfigType<const char*> {static TConfig get() {return TConfig::String;}};
template<> struct TConfigType<bool> {static TConfig get() {return TConfig::Bool;}};

} //end namespace ConfigHelpers


template<class K, class T>
ConfigConcrete<K,T>::ConfigConcrete() {
    key = nullptr;
    val = ConfigHelpers::getNull<T>();
}

template<class K, class T>
ConfigConcrete<K,T>::~ConfigConcrete() {
    ConfigHelpers::deinitKey(key);
}

template<class K, class T>
ConfigConcrete<K,T>::ConfigConcrete() {

}

template<class K, class T>
ConfigConcrete<K,T>::ConfigConcrete() {

}

template<class K, class T>
ConfigConcrete<K,T>::ConfigConcrete() {

}

template<class K, class T>
ConfigConcrete<K,T>::~ConfigConcrete() {
    ConfigHelpers::deinitKey(key);
}

template<class K, class T>
bool ConfigConcrete<K,T>::setKey(const char *key) {
    return ConfigHelpers::initKey(this->key, key);
}

template<class K, class T>
const char *ConfigConcrete<K,T>::getKey() {
    return key;
}

template<class K, class T>
int ConfigConcrete<K,T>::getInt() {
#if MOCPP_CONFIG_TYPECHECK
    if (getType() != TConfig::Int) {
        MOCPP_DBG_ERR("type err");
    }
#endif
    return ConfigHelpers::as<int>(val);
}

template<class K, class T>
bool ConfigConcrete<K,T>::getBool() {
#if MOCPP_CONFIG_TYPECHECK
    if (getType() != TConfig::Bool) {
        MOCPP_DBG_ERR("type err");
    }
#endif
    return ConfigHelpers::as<bool>(val);
}

template<class K, class T>
const char *ConfigConcrete<K,T>::getString() {
#if MOCPP_CONFIG_TYPECHECK
    if (getType() != TConfig::String) {
        MOCPP_DBG_ERR("type err");
    }
#endif
    return ConfigHelpers::as<const char*>(val);
}

template<class K, class T>
void ConfigConcrete<K,T>::setInt(int v) {
#if MOCPP_CONFIG_TYPECHECK
    if (getType() != TConfig::Int) {
        MOCPP_DBG_ERR("type err");
    }
#endif
    value_revision++;
    val = ConfigHelpers::as<T>(v);
}

template<class K, class T>
void ConfigConcrete<K,T>::setBool(bool v) {
#if MOCPP_CONFIG_TYPECHECK
    if (getType() != TConfig::Bool) {
        MOCPP_DBG_ERR("type err");
    }
#endif
    value_revision++;
    val = ConfigHelpers::as<T>(v);
}

template<class K, class T>
bool ConfigConcrete<K,T>::setString(const char *src) {
#if MOCPP_CONFIG_TYPECHECK
    if (getType() != TConfig::String) {
        MOCPP_DBG_ERR("type err");
    }
#endif
    value_revision++;
    auto dest = ConfigHelpers::as<char*>(val);
    if (dest) {
        free(dest);
        dest = nullptr;
    }
    if (src && dest) {
        size_t len = strlen(src);
        dest = (char*) malloc(len + 1);
        if (!dest) {
            return false;
        }
        strcpy(dest, src);
    }
    return true;
}

template<class K, class T>
TConfig ConfigConcrete<K,T>::getType() {
    return ConfigHelpers::TConfigType<T>::get();
}


bool deserializeTConfig(TConfig& out, const char *serialized) {
    if (!strcmp(serialized, "int")) {
        out = TConfig::Int;
    } else if (!strcmp(serialized, "bool")) {
        out = TConfig::Bool;
    } else if (!strcmp(serialized, "string")) {
        out = TConfig::String;
    } else {
        out = TConfig::None;
        MOCPP_DBG_WARN("config type error");
    }
    return out;
}

const char *serializeTConfig(TConfig type) {
    switch (type) {
        case (TConfig::Int):
            return "int";
        case (TConfig::Bool):
            return "bool";
        case (TConfig::String):
            return "string";
        case (TConfig::None):
            MOCPP_DBG_WARN("config type error");
            return "undefined";
    }
}

std::unique_ptr<Configuration> deserializeConfigInternal(JsonObject stored) {
    
    TConfig ctype = TConfig::None;
    if (!deserializeTConfig(ctype, stored["type"] | "_Undefined") || ctype == TConfig::None) {
        return nullptr;
    }

    if (!stored.containsKey("value")) {
        return nullptr;
    }

    std::unique_ptr<Configuration> res;

    switch (ctype) {
        case (TConfig::Int):
            if (!stored["value"].is<int>()) {
                return nullptr;
            }
            res = std::unique_ptr<ConfigConcrete<char*,int>>(new ConfigConcrete<char*,int>());
            if (!res) {
                return nullptr;
            }
            res->setInt(stored["value"] | 0);
            break;
        case (TConfig::Bool):
            if (!stored["value"].is<bool>()) {
                return nullptr;
            }
            res = std::unique_ptr<ConfigConcrete<char*,bool>>(new ConfigConcrete<char*,bool>());
            if (!res) {
                return nullptr;
            }
            res->setBool(stored["value"] | false);
            break;
        case (TConfig::String):
            if (!stored["value"].is<const char*>()) {
                return nullptr;
            }
            res = std::unique_ptr<ConfigConcrete<char*,const char*>>(new ConfigConcrete<char*,const char*>());
            if (!res) {
                return nullptr;
            }
            res->setString(stored["value"] | "");
            break;
        case (TConfig::None):
            return nullptr;
    }
    
    const char *key = stored["key"] | "";
    if (!*key) {
        return nullptr;
    }

    if (!res->setKey(key)) {
        return nullptr;
    }

    return res;
}

bool serializeConfigInternal(Configuration& config, JsonObject out) {

    out["key"] = config.getKey();
    
    switch (config.getType()) {
        case TConfig::Int:
            out["type"] = "int";
            out["value"] = config.getInt();
            break;
        case TConfig::Bool:
            out["type"] = "bool";
            out["value"] = config.getBool();
            break;
        case TConfig::String:
            out["type"] = "string";
            out["value"] = config.getString();
            break;
        case TConfig::None:
            MOCPP_DBG_ERR("invalid config");
            out.clear();
            return false;
            break;
    }

    return true;
}

bool serializeConfigOcpp(Configuration& config, DynamicJsonDocument& out) {

    const size_t PRIMITIVE_VALUESIZE = 30;
    char vbuf [PRIMITIVE_VALUESIZE];
    const char *v = vbuf;
    size_t vcapacity = 0;
    switch(config.getType()) {
        case TConfig::Int: {
            auto ret = snprintf(valuebuf, PRIMITIVE_VALUESIZE, "%i", config.getInt());
            if (ret < 0 || ret >= PRIMITIVE_VALUESIZE) {
                return false;
            }
            v = vbuf;
            vcapacity = ret;
            break;
        }
        case TConfig::Bool: {
            v = config.getBool() ? "true" : "false";
            vcapacity = 0;
            break;
        }
        case TConfig::String: {
            v = config.getString();
            if (!v) {
                MOCPP_DBG_ERR("invalid config");
                return nullptr;
            }
            vcapacity = 0;
            break;
        }
        case TConfig::None: {
            MOCPP_DBG_ERR("invalid config");
            return false;
        }
    }

    out.reset(JSON_OBJECT_SIZE(3) + vcapacity);

    out["key"] = config.getKey();
    out["readonly"] = !config.getWritePermission();
    if (vcapacity > 0) {
        //value has capacity, copy string
        out["value"] = (char*) v;
    } else {
        //value is static, no-copy mode
        out["value"] = v;
    }

    return true;
}

template class ConfigConcrete<char*,int>;
template class ConfigConcrete<char*,bool>;
template class ConfigConcrete<char*,const char*>;
template class ConfigConcrete<const char*,int>;
template class ConfigConcrete<const char*,bool>;
template class ConfigConcrete<const char*,const char*>;
//template class Configuration<const char *>; //no effect after explicit specialization

} //end namespace MicroOcpp
