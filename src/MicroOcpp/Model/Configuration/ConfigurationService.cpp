// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

/*
 * Implementation of the UCs B05 - B06
 */

#include <MicroOcpp/Model/Configuration/ConfigurationService.h>

#include <string.h>
#include <ctype.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Operations/ChangeConfiguration.h>
#include <MicroOcpp/Operations/GetConfiguration.h>
#include <MicroOcpp/Operations/GetBaseReport.h>
#include <MicroOcpp/Operations/NotifyReport.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16

#ifndef MO_GETBASEREPORT_CHUNKSIZE
#define MO_GETBASEREPORT_CHUNKSIZE 4
#endif

namespace MicroOcpp {
namespace v16 {

template <class T>
ConfigurationValidator<T>::ConfigurationValidator(const char *key, bool (*validateFn)(T, void*), void *user_data) :
        key(key), user_data(user_data), validateFn(validateFn) {

}

template <class T>
bool ConfigurationValidator<T>::validate(T v) {
    return validateFn(v, user_data);
}

bool VALIDATE_UNSIGNED_INT(int v, void*) {
    return v >= 0;
}

template <class T>
ConfigurationValidator<T> *getConfigurationValidator(Vector<ConfigurationValidator<T>>& collection, const char *key) {
    for (size_t i = 0; i < collection.size(); i++) {
        auto& validator = collection[i];
        if (!strcmp(key, validator.key)) {
            return &validator;
        }
    }
    return nullptr;
}

ConfigurationValidator<int> *ConfigurationService::getValidatorInt(const char *key) {
    return getConfigurationValidator<int>(validatorInt, key);
}

ConfigurationValidator<bool> *ConfigurationService::getValidatorBool(const char *key) {
    return getConfigurationValidator<bool>(validatorBool, key);
}

ConfigurationValidator<const char*> *ConfigurationService::getValidatorString(const char *key) {
    return getConfigurationValidator<const char*>(validatorString, key);
}

ConfigurationContainerOwning *ConfigurationService::getContainerInternal(const char *filename) {
    for (size_t i = 0; i < containersInternal.size(); i++) {
        auto container = containersInternal[i];
        if (container->getFilename() && !strcmp(filename, container->getFilename())) {
            return container;
        }
    }
    return nullptr;
}

ConfigurationContainerOwning *ConfigurationService::declareContainer(const char *filename) {

    auto container = getContainerInternal(filename);

    if (!container) {
        MO_DBG_DEBUG("init new configurations container: %s", filename);

        container = new ConfigurationContainerOwning();
        if (!container) {
            MO_DBG_ERR("OOM");
            return nullptr;
        }
        if (!container->setFilename(filename)) {
            MO_DBG_ERR("OOM");
            delete container;
            container = nullptr;
            return nullptr;
        }
        containersInternal.push_back(container); //transfer ownership
        containers.push_back(container); //does not take ownership
    }

    return container;
}

bool ConfigurationService::addContainer(ConfigurationContainer *container) {
    containers.push_back(container); //does not take ownership
    return true;
}

template <class T>
bool registerConfigurationValidator(Vector<ConfigurationValidator<T>>& collection, const char *key, bool (*validate)(T, void*), void *user_data) {
    for (auto it = collection.begin(); it != collection.end(); it++) {
        if (!strcmp(key, it->key)) {
            collection.erase(it);
            break;
        }
    }
    collection.emplace_back(key, validate, user_data);
    return true;
}

template <>
bool ConfigurationService::registerValidator<int>(const char *key, bool (*validate)(int, void*), void *user_data) {
    return registerConfigurationValidator<int>(validatorInt, key, validate, user_data);
}

template <>
bool ConfigurationService::registerValidator<bool>(const char *key, bool (*validate)(bool, void*), void *user_data) {
    return registerConfigurationValidator<bool>(validatorBool, key, validate, user_data);
}

template <>
bool ConfigurationService::registerValidator<const char*>(const char *key, bool (*validate)(const char*, void*), void *user_data) {
    return registerConfigurationValidator<const char*>(validatorString, key, validate, user_data);
}

Configuration *ConfigurationService::getConfiguration(const char *key) {

    for (size_t i = 0; i < containers.size(); i++) {
        auto container = containers[i];
        if (auto config = container->getConfiguration(key)) {
            return config;
        }
    }

    return nullptr;
}

bool ConfigurationService::getConfiguration(const char **keys, size_t keysSize, Vector<Configuration*>& configurations, Vector<char*>& unknownKeys) {
    if (keysSize == 0) {
        // query empty - dump all configs

        for (size_t i = 0; i < containers.size(); i++) {
            auto container = containers[i];
            for (size_t j = 0; j < container->size(); j++) {
                auto config = container->getConfiguration(j);
                if (config->getMutability() == Mutability::ReadWrite || config->getMutability() == Mutability::ReadOnly) {
                    configurations.push_back(config); //does not take ownership
                }
            }
        }
    } else {
        // query configs by keys

        for (size_t i = 0; i < keysSize; i++) {
            const char *key = keys[i];

            auto config = getConfiguration(key);
            if (config && (config->getMutability() == Mutability::ReadWrite || config->getMutability() == Mutability::ReadOnly)) {
                //found key
                configurations.push_back(config); //does not take ownership
            } else {
                //unknown key

                size_t size = strlen(key) + 1;
                char *unknownKey = static_cast<char*>(MO_MALLOC(getMemoryTag(), size));
                if (!unknownKey) {
                    MO_DBG_ERR("OOM");
                    return false;
                }
                memset(unknownKey, 0, size);
                auto ret = snprintf(unknownKey, size, "%s", key);
                if (ret < 0 || (size_t)ret >= size) {
                    MO_DBG_ERR("snprintf: %i", ret);
                    MO_FREE(unknownKey);
                    unknownKey = nullptr;
                    return false;
                }

                unknownKeys.push_back(unknownKey); //transfer ownership
            }
        }
    }

    return true;
}

ConfigurationStatus ConfigurationService::changeConfiguration(const char *key, const char *value) {

    ConfigurationContainer *container = nullptr;
    Configuration *config = nullptr;

    for (size_t i = 0; i < containers.size(); i++) {
        auto container_i = containers[i];
        for (size_t j = 0; j < container_i->size(); j++) {
            auto config_ij = container_i->getConfiguration(j);
            if (!strcmp(key, config_ij->getKey())) {
                container = container_i;
                config = config_ij;
            }
        }
    }

    if (!config || config->getMutability() == Mutability::None) {
        //config not found or hidden config
        return ConfigurationStatus::NotSupported;
    }

    if (config->getMutability() == Mutability::ReadOnly) {
        MO_DBG_WARN("Trying to override readonly value");
        return ConfigurationStatus::Rejected;
    }

    //write config

    /*
    * Try to interpret input as number
    */

    bool convertibleInt = true;
    int numInt = 0;
    bool convertibleBool = true;
    bool numBool = false;

    int nDigits = 0, nNonDigits = 0, nDots = 0, nSign = 0; //"-1.234" has 4 digits, 0 nonDigits, 1 dot and 1 sign. Don't allow comma as seperator. Don't allow e-expressions (e.g. 1.23e-7)
    for (const char *c = value; *c; ++c) {
        if (*c >= '0' && *c <= '9') {
            //int interpretation
            if (nDots == 0) { //only append number if before floating point
                nDigits++;
                numInt *= 10;
                numInt += *c - '0';
            }
        } else if (*c == '.') {
            nDots++;
        } else if (c == value && *c == '-') {
            nSign++;
        } else {
            nNonDigits++;
        }
    }

    if (nSign == 1) {
        numInt = -numInt;
    }

    int INT_MAXDIGITS; //plausibility check: this allows a numerical range of (-999,999,999 to 999,999,999), or (-9,999 to 9,999) respectively
    if (sizeof(int) >= 4UL)
        INT_MAXDIGITS = 9;
    else
        INT_MAXDIGITS = 4;

    if (nNonDigits > 0 || nDigits == 0 || nSign > 1 || nDots > 1) {
        convertibleInt = false;
    }

    if (nDigits > INT_MAXDIGITS) {
        MO_DBG_DEBUG("Possible integer overflow: key = %s, value = %s", key, value);
        convertibleInt = false;
    }

    if (tolower(value[0]) == 't' && tolower(value[1]) == 'r' && tolower(value[2]) == 'u' && tolower(value[3]) == 'e' && !value[4]) {
        numBool = true;
    } else if (tolower(value[0]) == 'f' && tolower(value[1]) == 'a' && tolower(value[2]) == 'l' && tolower(value[3]) == 's' && tolower(value[4]) == 'e' && !value[5]) {
        numBool = false;
    } else {
        convertibleBool = false;
    }

    //validate and store (parsed) value to Config

    if (config->getType() == Configuration::Type::Int && convertibleInt) {
        auto validator = getValidatorInt(key);
        if (validator && !validator->validate(numInt)) {
            MO_DBG_WARN("validation failed for key=%s value=%i", key, numInt);
            return ConfigurationStatus::Rejected;
        }
        config->setInt(numInt);
    } else if (config->getType() == Configuration::Type::Bool && convertibleBool) {
        auto validator = getValidatorBool(key);
        if (validator && !validator->validate(numBool)) {
            MO_DBG_WARN("validation failed for key=%s value=%s", key, numBool ? "true" : "false");
            return ConfigurationStatus::Rejected;
        }
        config->setBool(numBool);
    } else if (config->getType() == Configuration::Type::String) {
        auto validator = getValidatorString(key);
        if (validator && !validator->validate(value)) {
            MO_DBG_WARN("validation failed for key=%s value=%i", key, value);
            return ConfigurationStatus::Rejected;
        }
        if (!config->setString(value)) {
            MO_DBG_WARN("OOM");
            return ConfigurationStatus::Rejected;
        }
    } else {
        MO_DBG_WARN("Value has incompatible type");
        return ConfigurationStatus::Rejected;
    }

    if (!container->commit()) {
        MO_DBG_ERR("could not write changes to flash");
        return ConfigurationStatus::Rejected;
    }

    //success

    if (config->isRebootRequired()) {
        return ConfigurationStatus::RebootRequired;
    } else {
        return ConfigurationStatus::Accepted;
    }
}

ConfigurationContainer *ConfigurationService::findContainerOfConfiguration(Configuration *whichConfig) {
    for (size_t i = 0; i < containers.size(); i++) {
        auto container = containers[i];
        for (size_t j = 0; j < container->size(); j++) {
            if (whichConfig == container->getConfiguration(j)) {
                return container;
            }
        }
    }
    return nullptr;
}

ConfigurationService::ConfigurationService(Context& context) :
            MemoryManaged("v16.Configuration.ConfigurationService"),
            context(context),
            containers(makeVector<ConfigurationContainer*>(getMemoryTag())),
            validatorInt(makeVector<ConfigurationValidator<int>>(getMemoryTag())),
            validatorBool(makeVector<ConfigurationValidator<bool>>(getMemoryTag())),
            validatorString(makeVector<ConfigurationValidator<const char*>>(getMemoryTag())) {

}

ConfigurationService::~ConfigurationService() {
    for (unsigned int i = 0; i < containersInternal.size(); i++) {
        delete containersInternal[i];
    }
}
            
bool ConfigurationService::init() {
    containers.reserve(1);
    if (containers.capacity() < 1) {
        MO_DBG_ERR("OOM");
        return false;
    }

    containers.push_back(&containerExternal);
    return true;
}

bool ConfigurationService::setup() {

    declareConfiguration<int>("GetConfigurationMaxKeys", 30, MO_CONFIGURATION_VOLATILE, Mutability::ReadOnly);

    filesystem = context.getFilesystem();
    if (!filesystem) {
        MO_DBG_DEBUG("no fs access");
    }

    for (unsigned int i = 0; i < containersInternal.size(); i++) {
        containersInternal[i]->setFilesystem(filesystem);
        if (!containersInternal[i]->load()) {
            MO_DBG_ERR("failure to load %s", containersInternal[i]->getFilename());
            return false;
        }
    }

    context.getMessageService().registerOperation("ChangeConfiguration", [] (Context& context) -> Operation* {
        return new ChangeConfiguration(*context.getModel16().getConfigurationService());});
    context.getMessageService().registerOperation("GetConfiguration", [] (Context& context) -> Operation* {
        return new GetConfiguration(*context.getModel16().getConfigurationService());});

    return true;
}

template<class T>
bool loadConfigurationFactoryDefault(Configuration& config, T factoryDef);

template<>
bool loadConfigurationFactoryDefault<int>(Configuration& config, int factoryDef) {
    config.setInt(factoryDef);
    return true;
}

template<>
bool loadConfigurationFactoryDefault<bool>(Configuration& config, bool factoryDef) {
    config.setBool(factoryDef);
    return true;
}

template<>
bool loadConfigurationFactoryDefault<const char*>(Configuration& config, const char *factoryDef) {
    return config.setString(factoryDef);
}

void loadConfigurationCharacteristics(Configuration& config, Mutability mutability, bool rebootRequired) {
    if (config.getMutability() == Mutability::ReadWrite) {
        config.setMutability(mutability);
    } else if (mutability != config.getMutability() && mutability != Mutability::ReadWrite) {
        config.setMutability(Mutability::None);
    }

    if (rebootRequired) {
        config.setRebootRequired();
    }
}

template<class T>
Configuration::Type getType();

template<> Configuration::Type getType<int>() {return Configuration::Type::Int;}
template<> Configuration::Type getType<bool>() {return Configuration::Type::Bool;}
template<> Configuration::Type getType<const char*>() {return Configuration::Type::String;}

template<class T>
Configuration *ConfigurationService::declareConfiguration(const char *key, T factoryDefault, const char *filename, Mutability mutability, bool rebootRequired) {

    auto res = getConfiguration(key);
    if (!res) {
        auto container = declareContainer(filename);
        if (!container) {
            MO_DBG_ERR("OOM");
            return nullptr;
        }

        auto config = makeConfiguration(getType<T>());
        if (!config) {
            MO_DBG_ERR("OOM");
            return nullptr;
        }

        config->setKey(key);

        if (!loadConfigurationFactoryDefault<T>(*config, factoryDefault)) {
            return nullptr;
        }

        res = config.get();

        if (!container->add(std::move(config))) {
            return nullptr;
        }
    }

    loadConfigurationCharacteristics(*res, mutability, rebootRequired);
    return res;
}

template Configuration *ConfigurationService::declareConfiguration<int>(        const char*, int,         const char*, Mutability, bool);
template Configuration *ConfigurationService::declareConfiguration<bool>(       const char*, bool,        const char*, Mutability, bool);
template Configuration *ConfigurationService::declareConfiguration<const char*>(const char*, const char*, const char*, Mutability, bool);

bool ConfigurationService::addConfiguration(Configuration *config) {
    return containerExternal.add(config);
}

bool ConfigurationService::addConfiguration(std::unique_ptr<Configuration> config, const char *filename) {
    if (getConfiguration(config->getKey())) {
        MO_DBG_ERR("config already exists");
        return false;
    }
    auto container = declareContainer(filename);
    if (!container) {
        MO_DBG_ERR("OOM");
        return false;
    }
    return container->add(std::move(config));
}

bool ConfigurationService::commit() {
    bool success = true;

    for (size_t i = 0; i < containers.size(); i++) {
        if (!containers[i]->commit()) {
            success = false;
        }
    }

    return success;
}

} //namespace v16
} //namespace MicroOcpp

#endif //MO_ENABLE_V16
