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

template<> TConfig convertType<int>() {return TConfig::Int;}
template<> TConfig convertType<bool>() {return TConfig::Bool;}
template<> TConfig convertType<const char*>() {return TConfig::String;}

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

void Configuration::setInt(int) {
#if MOCPP_CONFIG_TYPECHECK
    MOCPP_DBG_ERR("type err");
#endif
}

void Configuration::setBool(bool) {
#if MOCPP_CONFIG_TYPECHECK
    MOCPP_DBG_ERR("type err");
#endif
}

bool Configuration::setString(const char*) {
#if MOCPP_CONFIG_TYPECHECK
    MOCPP_DBG_ERR("type err");
#endif
}

int Configuration::getInt() {
#if MOCPP_CONFIG_TYPECHECK
    MOCPP_DBG_ERR("type err");
#endif
    return 0;
}

bool Configuration::getBool() {
#if MOCPP_CONFIG_TYPECHECK
    MOCPP_DBG_ERR("type err");
#endif
    return false;
}

const char *Configuration::getString() {
#if MOCPP_CONFIG_TYPECHECK
    MOCPP_DBG_ERR("type err");
#endif
    return nullptr;
}

revision_t Configuration::getValueRevision() {
    return value_revision;
}

void Configuration::setRebootRequired() {
    rebootRequired = true;
}

bool Configuration::isRebootRequired() {
    return rebootRequired;
}

void Configuration::setReadOnly() {
    readOnly = true;
}

bool Configuration::isReadOnly() {
    return readOnly;
}

/*
 * Default implementations of the Configuration interface.
 *
 * How to use custom implementations: for each OCPP config, pass a config instance to the OCPP lib
 * before its initialization stage. Then the library won't create new config objects but 
 */

class ConfigInt : public Configuration {
private:
    const char *key = nullptr;
    int val = 0;
public:

    ~ConfigInt() = default;

    bool setKey(const char *key) override {
        this->key = key;
        return true;
    }

    const char *getKey() override {
        return key;
    }

    TConfig getType() override {
        return TConfig::Int;
    }

    void setInt(int val) override {
        this->val = val;
    }

    int getInt() override {
        return val;
    }
};

class ConfigBool : public Configuration {
private:
    const char *key = nullptr;
    bool val = false;
public:

    ~ConfigBool() = default;

    bool setKey(const char *key) override {
        this->key = key;
        return true;
    }

    const char *getKey() override {
        return key;
    }

    TConfig getType() override {
        return TConfig::Bool;
    }

    void setBool(bool val) override {
        this->val = val;
    }

    bool getBool() override {
        return val;
    }
};

class ConfigString : public Configuration {
private:
    const char *key = nullptr;
    char *val = nullptr;
public:
    ConfigString() = default;
    ConfigString(const ConfigString&) = delete;
    ConfigString& operator=(const ConfigString&) = delete;

    ~ConfigString() {
        free(val);
    }

    bool setKey(const char *key) override {
        this->key = key;
        return true;
    }

    const char *getKey() override {
        return key;
    }

    TConfig getType() override {
        return TConfig::String;
    }

    bool setString(const char *src) override {
        if (!this->val && !src) {
            return true;
        }

        if (this->val && src && !strcmp(this->val, src)) {
            return true;
        }

        size_t size = 0;
        if (src) {
            size = strlen(src) + 1;
        }

        if (size > MOCPP_CONFIG_MAX_VALSTRSIZE) {
            return false;
        }

        value_revision++;

        if (this->val) {
            free(this->val);
            this->val = nullptr;
        }

        if (src) {
            this->val = (char*) malloc(size);
            if (!this->val) {
                return false;
            }
            strcpy(this->val, src);
        }

        return true;
    }

    const char *getString() override {
        return val;
    }
};

template<class T>
std::unique_ptr<Configuration> makeConfig(const char *key, T val) {
    std::unique_ptr<ConfigInt> res;
    switch (convertType<T>()) {
        case TConfig::Int:
            res = std::unique_ptr<ConfigInt>(new ConfigInt());
            if (res) res->setInt(val);
            break;
        case TConfig::Bool:
            res = std::unique_ptr<ConfigBool>(new ConfigBool());
            if (res) res->setBool(val);
            break;
        case TConfig::String:
            res = std::unique_ptr<ConfigString>(new ConfigString());
            if (res) res->setString(val);
            break;
    }
    if (!res) {
        return nullptr;
    }
    res->setKey(key);
    return res;
}

template<> std::unique_ptr<Configuration> makeConfig<int>(const char *key, int val);
template<> std::unique_ptr<Configuration> makeConfig<bool>(const char *key, bool val);
template<> std::unique_ptr<Configuration> makeConfig<const char*>(const char *key, const char *val);

bool deserializeTConfig(const char *serialized, TConfig& out) {
    if (!strcmp(serialized, "int")) {
        out = TConfig::Int;
        return true;
    } else if (!strcmp(serialized, "bool")) {
        out = TConfig::Bool;
        return true;
    } else if (!strcmp(serialized, "string")) {
        out = TConfig::String;
        return true;
    } else {
        MOCPP_DBG_WARN("config type error");
        return false;
    }
}

const char *serializeTConfig(TConfig type) {
    switch (type) {
        case (TConfig::Int):
            return "int";
        case (TConfig::Bool):
            return "bool";
        case (TConfig::String):
            return "string";
    }
}

bool serializeConfigOcpp(Configuration& config, DynamicJsonDocument& out) {

    if (!config.getKey()) {
        return false;
    }
    
    const size_t PRIMITIVE_VALUESIZE = 30;
    char vbuf [PRIMITIVE_VALUESIZE];
    const char *v = vbuf;
    size_t vcapacity = 0;
    switch(config.getType()) {
        case TConfig::Int: {
            auto ret = snprintf(vbuf, PRIMITIVE_VALUESIZE, "%i", config.getInt());
            if (ret < 0 || ret >= PRIMITIVE_VALUESIZE) {
                return false;
            }
            v = vbuf;
            vcapacity = ret + 1;
            break;
        }
        case TConfig::Bool:
            v = config.getBool() ? "true" : "false";
            vcapacity = 0;
            break;
        case TConfig::String:
            v = config.getString();
            if (!v) {
                MOCPP_DBG_ERR("invalid config");
                return false;
            }
            vcapacity = 0;
            break;
    }

    out = DynamicJsonDocument(JSON_OBJECT_SIZE(3) + vcapacity);

    out["key"] = config.getKey();
    out["readonly"] = config.isReadOnly();
    if (vcapacity > 0) {
        //value has capacity, copy string
        out["value"] = (char*) v;
    } else {
        //value is static, no-copy mode
        out["value"] = v;
    }

    return true;
}


} //end namespace MicroOcpp
