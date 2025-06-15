// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#include <string.h>

#include <MicroOcpp/Model/Configuration/Configuration.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16

using namespace MicroOcpp;
using namespace MicroOcpp::Ocpp16;

Configuration::~Configuration() {

}

void Configuration::setKey(const char *name) {
    this->key = name;
    updateMemoryTag("v16.Configuration.", name);
}
const char *Configuration::getKey() const {
    return key;
}

void Configuration::setInt(int val) {
    MO_DBG_ERR("type err");
}
void Configuration::setBool(bool val) {
    MO_DBG_ERR("type err");
}
bool Configuration::setString(const char *val) {
    MO_DBG_ERR("type err");
    return false;
}

int Configuration::getInt() {
    MO_DBG_ERR("type err");
    return 0;
}
bool Configuration::getBool() {
    MO_DBG_ERR("type err");
    return false;
}
const char *Configuration::getString() {
    MO_DBG_ERR("type err");
    return "";
}

uint16_t Configuration::getWriteCount() {
    return 0;
}

bool Configuration::isRebootRequired() {
    return rebootRequired;
}
void Configuration::setRebootRequired() {
    rebootRequired = true;
}

void Configuration::setMutability(Mutability mutability) {
    this->mutability = mutability;
}

Mutability Configuration::getMutability() {
    return mutability;
}

class ConfigurationConcrete : public Configuration {
private:
    uint16_t writeCount = 0;

    union {
        int valueInt;
        bool valueBool;
        char *valueString = nullptr;
    };
    Type type = Type::Int;

    bool resetString(const char *val) {
        if (type != Type::String) {
            return false;
        }

        char *nextString = nullptr;

        if (val) {
            size_t size = strlen(val) + 1;
            nextString = static_cast<char*>(MO_MALLOC(getMemoryTag(), size));
            if (!nextString) {
                MO_DBG_ERR("OOM");
                return false;
            }
            memset(nextString, 0, size);
            auto ret = snprintf(nextString, size, "%s", val);
            if (ret < 0 || (size_t)ret >= size) {
                MO_DBG_ERR("snprintf: %i", ret);
                MO_FREE(nextString);
                nextString = nullptr;
                return false;
            }
        }

        MO_FREE(valueString);
        valueString = nextString;
        return true;
    }

public:
    ConfigurationConcrete(Type type) : type(type) { }
    ~ConfigurationConcrete() {
        (void)resetString(nullptr);
    }

    void setInt(int val) override {
        #if MO_CONFIG_TYPECHECK
        if (type != Type::Int) {
            MO_DBG_ERR("type err: %s is not int", getKey());
            return;
        }
        #endif //MO_CONFIG_TYPECHECK
        valueInt = val;
        writeCount++;
    }
    void setBool(bool val) override {
        #if MO_CONFIG_TYPECHECK
        if (type != Type::Bool) {
            MO_DBG_ERR("type err: %s is not bool", getKey());
            return;
        }
        #endif //MO_CONFIG_TYPECHECK
        valueBool = val;
        writeCount++;
    }
    bool setString(const char *val) override {
        #if MO_CONFIG_TYPECHECK
        if (type != Type::String) {
            MO_DBG_ERR("type err: %s is not string", getKey());
            return false;
        }
        #endif //MO_CONFIG_TYPECHECK
        bool success = resetString(val);
        if (success) {
            writeCount++;
        }
        return success;
    }

    // get Value of Configuration
    int getInt() override {
        #if MO_CONFIG_TYPECHECK
        if (type != Type::Int) {
            MO_DBG_ERR("type err: %s is not int", getKey());
            return 0;
        }
        #endif //MO_CONFIG_TYPECHECK
        return valueInt;
    }
    bool getBool() override {
        #if MO_CONFIG_TYPECHECK
        if (type != Type::Bool) {
            MO_DBG_ERR("type err: %s is not bool", getKey());
            return false;
        }
        #endif //MO_CONFIG_TYPECHECK
        return valueBool;
    }
    const char *getString() override {
        #if MO_CONFIG_TYPECHECK
        if (type != Type::String) {
            MO_DBG_ERR("type err: %s is not string", getKey());
            return "";
        }
        #endif //MO_CONFIG_TYPECHECK
        return valueString ? valueString : "";
    }

    Type getType() override {
        return type;
    }

    uint16_t getWriteCount() override {
        return writeCount;
    }
};

std::unique_ptr<Configuration> MicroOcpp::Ocpp16::makeConfiguration(Configuration::Type type) {
    return std::unique_ptr<Configuration>(new ConfigurationConcrete(type));
}

#endif //MO_ENABLE_V16
