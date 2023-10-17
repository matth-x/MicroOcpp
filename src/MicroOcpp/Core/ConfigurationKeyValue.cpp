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

#ifndef MO_CONFIG_TYPECHECK
#define MO_CONFIG_TYPECHECK 1 //enable this for debugging
#endif

namespace MicroOcpp {

template<> TConfig convertType<int>() {return TConfig::Int;}
template<> TConfig convertType<bool>() {return TConfig::Bool;}
template<> TConfig convertType<const char*>() {return TConfig::String;}

Configuration::~Configuration() {

}

void Configuration::setInt(int) {
#if MO_CONFIG_TYPECHECK
    MO_DBG_ERR("type err");
#endif
}

void Configuration::setBool(bool) {
#if MO_CONFIG_TYPECHECK
    MO_DBG_ERR("type err");
#endif
}

bool Configuration::setString(const char*) {
#if MO_CONFIG_TYPECHECK
    MO_DBG_ERR("type err");
#endif
    return false;
}

int Configuration::getInt() {
#if MO_CONFIG_TYPECHECK
    MO_DBG_ERR("type err");
#endif
    return 0;
}

bool Configuration::getBool() {
#if MO_CONFIG_TYPECHECK
    MO_DBG_ERR("type err");
#endif
    return false;
}

const char *Configuration::getString() {
#if MO_CONFIG_TYPECHECK
    MO_DBG_ERR("type err");
#endif
    return "";
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
        value_revision++;
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
        value_revision++;
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
    ConfigString(ConfigString&&) = delete;
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
        bool src_empty = !src || !*src;

        if (!val && src_empty) {
            return true;
        }

        if (this->val && src && !strcmp(this->val, src)) {
            return true;
        }

        size_t size = 0;
        if (!src_empty) {
            size = strlen(src) + 1;
        }

        if (size > MO_CONFIG_MAX_VALSTRSIZE) {
            return false;
        }

        value_revision++;

        if (this->val) {
            free(this->val);
            this->val = nullptr;
        }

        if (!src_empty) {
            this->val = (char*) malloc(size);
            if (!this->val) {
                return false;
            }
            strcpy(this->val, src);
        }

        return true;
    }

    const char *getString() override {
        if (!val) {
            return "";
        }
        return val;
    }
};

std::unique_ptr<Configuration> makeConfiguration(TConfig type, const char *key) {
    std::unique_ptr<Configuration> res;
    switch (type) {
        case TConfig::Int:
            res.reset(new ConfigInt());
            break;
        case TConfig::Bool:
            res.reset(new ConfigBool());
            break;
        case TConfig::String:
            res.reset(new ConfigString());
            break;
    }
    if (!res) {
        MO_DBG_ERR("OOM");
        return nullptr;
    }
    res->setKey(key);
    return res;
};

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
        MO_DBG_WARN("config type error");
        return false;
    }
}

const char *serializeTConfig(TConfig type) {
    switch (type) {
        case TConfig::Int:
            return "int";
        case TConfig::Bool:
            return "bool";
        case TConfig::String:
            return "string";
    }
    return "_Undefined";
}

} //end namespace MicroOcpp
