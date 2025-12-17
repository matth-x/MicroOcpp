// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/ConfigurationContainerFlash.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Debug.h>

#include <string.h>
#include <algorithm>
#include <climits>
#include <ArduinoJson.h>

namespace MicroOcpp {

struct Validator {
    const char *key = nullptr;
    std::function<bool(const char*)> checkValue;
    Validator(const char *key, std::function<bool(const char*)> checkValue) : key(key), checkValue(checkValue) {

    }
};

namespace ConfigurationLocal {

std::shared_ptr<FilesystemAdapter> filesystem;
auto configurationContainers = makeVector<std::shared_ptr<ConfigurationContainer>>("v16.Configuration.Containers");
auto validators = makeVector<Validator>("v16.Configuration.Validators");

}

using namespace ConfigurationLocal;

std::unique_ptr<ConfigurationContainer> createConfigurationContainer(const char *filename, bool accessible) {
    //create non-persistent Configuration store (i.e. lives only in RAM) if
    //     - Flash FS usage is switched off OR
    //     - Filename starts with "/volatile"
    if (!filesystem ||
                 !strncmp(filename, CONFIGURATION_VOLATILE, strlen(CONFIGURATION_VOLATILE))) {
        return makeConfigurationContainerVolatile(filename, accessible);
    } else {
        //create persistent Configuration store. This is the normal case
        return makeConfigurationContainerFlash(filesystem, filename, accessible);
    }
}


void addConfigurationContainer(std::shared_ptr<ConfigurationContainer> container) {
    configurationContainers.push_back(container);
}

std::shared_ptr<ConfigurationContainer> getContainer(const char *filename) {
    auto container = std::find_if(configurationContainers.begin(), configurationContainers.end(),
        [filename](decltype(configurationContainers)::value_type &elem) {
            return !strcmp(elem->getFilename(), filename);
        });

    if (container != configurationContainers.end()) {
        return *container;
    } else {
        return nullptr;
    }
}

ConfigurationContainer *declareContainer(const char *filename, bool accessible) {

    auto container = getContainer(filename);
    
    if (!container) {
        MO_DBG_DEBUG("init new configurations container: %s", filename);

        container = createConfigurationContainer(filename, accessible);
        if (!container) {
            MO_DBG_ERR("OOM");
            return nullptr;
        }
        configurationContainers.push_back(container);
    }

    if (container->isAccessible() != accessible) {
        MO_DBG_ERR("%s: conflicting accessibility declarations (expect %s)", filename, container->isAccessible() ? "accessible" : "inaccessible");
    }

    return container.get();
}

std::shared_ptr<Configuration> loadConfiguration(TConfig type, const char *key, bool accessible) {
    for (auto& container : configurationContainers) {
        if (auto config = container->getConfiguration(key)) {
            if (config->getType() != type) {
                MO_DBG_ERR("conflicting type for %s - remove old config", key);
                container->remove(config.get());
                continue;
            }
            if (container->isAccessible() != accessible) {
                MO_DBG_ERR("conflicting accessibility for %s", key);
            }
            container->loadStaticKey(*config.get(), key);
            return config;
        }
    }
    return nullptr;
}

template<class T>
bool loadFactoryDefault(Configuration& config, T loadFactoryDefault);

template<>
bool loadFactoryDefault<int>(Configuration& config, int factoryDef) {
    config.setInt(factoryDef);
    return true;
}

template<>
bool loadFactoryDefault<bool>(Configuration& config, bool factoryDef) {
    config.setBool(factoryDef);
    return true;
}

template<>
bool loadFactoryDefault<const char*>(Configuration& config, const char *factoryDef) {
    return config.setString(factoryDef);
}

void loadPermissions(Configuration& config, bool readonly, bool rebootRequired) {
    if (readonly) {
        config.setReadOnly();
    }

    if (rebootRequired) {
        config.setRebootRequired();
    }
}

template<class T>
std::shared_ptr<Configuration> declareConfiguration(const char *key, T factoryDef, const char *filename, bool readonly, bool rebootRequired, bool accessible) {

    std::shared_ptr<Configuration> res = loadConfiguration(convertType<T>(), key, accessible);
    if (!res) {
        auto container = declareContainer(filename, accessible);
        if (!container) {
            return nullptr;
        }

        res = container->createConfiguration(convertType<T>(), key);
        if (!res) {
            return nullptr;
        }

        if (!loadFactoryDefault(*res.get(), factoryDef)) {
            container->remove(res.get());
            return nullptr;
        }
    }

    loadPermissions(*res.get(), readonly, rebootRequired);
    return res;
}

template std::shared_ptr<Configuration> declareConfiguration<int>(const char *key, int factoryDef, const char *filename, bool readonly, bool rebootRequired, bool accessible);
template std::shared_ptr<Configuration> declareConfiguration<bool>(const char *key, bool factoryDef, const char *filename, bool readonly, bool rebootRequired, bool accessible);
template std::shared_ptr<Configuration> declareConfiguration<const char*>(const char *key, const char *factoryDef, const char *filename, bool readonly, bool rebootRequired, bool accessible);

std::function<bool(const char*)> *getConfigurationValidator(const char *key) {
    for (auto& v : validators) {
        if (!strcmp(v.key, key)) {
            return &v.checkValue;
        }
    }
    return nullptr;
}

void registerConfigurationValidator(const char *key, std::function<bool(const char*)> validator) {
    for (auto& v : validators) {
        if (!strcmp(v.key, key)) {
            v.checkValue = validator;
            return;
        }
    }
    validators.push_back(Validator{key, validator});
}

Configuration *getConfigurationPublic(const char *key) {
    for (auto& container : configurationContainers) {
        if (container->isAccessible()) {
            if (auto res = container->getConfiguration(key)) {
                return res.get();
            }
        }
    }

    return nullptr;
}

Vector<ConfigurationContainer*> getConfigurationContainersPublic() {
    auto res = makeVector<ConfigurationContainer*>("v16.Configuration.Containers");

    for (auto& container : configurationContainers) {
        if (container->isAccessible()) {
            res.push_back(container.get());
        }
    }

    return res;
}

bool configuration_init(std::shared_ptr<FilesystemAdapter> _filesystem) {
    filesystem = _filesystem;
    return true;
}

void configuration_deinit() {
    makeVector<decltype(configurationContainers)::value_type>("v16.Configuration.Containers").swap(configurationContainers); //release allocated memory (see https://cplusplus.com/reference/vector/vector/clear/)
    makeVector<decltype(validators)::value_type>("v16.Configuration.Validators").swap(validators);
    filesystem.reset();
}

bool configuration_load(const char *filename) {
    bool success = true;

    for (auto& container : configurationContainers) {
        if ((!filename || !strcmp(filename, container->getFilename())) && !container->load()) {
            success = false;
        }
    }

    return success;
}

bool configuration_save() {
    bool success = true;

    for (auto& container : configurationContainers) {
        if (!container->save()) {
            success = false;
        }
    }

    return success;
}

bool configuration_clean_unused() {
    for (auto& container : configurationContainers) {
        container->removeUnused();
    }
    return configuration_save();
}

bool VALIDATE_UNSIGNED_INT(const char *value) {
    for(size_t i = 0; value[i] != '\0'; i++) {
        if (value[i] < '0' || value[i] > '9') {
            return false;
        }
    }
    return true;
}

bool safeStringToInt(const char* str, int* result) {
    if (!str || !result) {
        return false;
    }

    // Skip leading whitespace
    while (*str == ' ' || *str == '\t') {
        str++;
    }

    // Check for empty string
    if (*str == '\0') {
        return false;
    }

    // Handle sign
    bool negative = false;
    if (*str == '-') {
        negative = true;
        str++;
    } else if (*str == '+') {
        str++;
    }

    // Check if there are digits after sign
    if (*str < '0' || *str > '9') {
        return false;
    }

    // Count digits and validate characters BEFORE parsing
    int nDigits = 0;
    const char* p = str;
    while (*p >= '0' && *p <= '9') {
        nDigits++;
        p++;
    }

    // Check for trailing non-digit characters (allow decimal point for compatibility)
    if (*p != '\0' && *p != '.') {
        return false;
    }

    // Determine maximum safe digit count based on int size
    int INT_MAXDIGITS;
    if (sizeof(int) >= 4UL) {
        INT_MAXDIGITS = 10;  // Safe range: allows valid 10-digit integers within INT_MAX
    } else {
        INT_MAXDIGITS = 4;  // Safe range: -9,999 to 9,999
    }

    // Reject if too many digits (prevents overflow)
    if (nDigits > INT_MAXDIGITS) {
        return false;
    }

    // Now perform the actual conversion with overflow checking
    int value = 0;
    const int INT_MAX_DIV_10 = INT_MAX / 10;
    const int INT_MIN_DIV_10 = INT_MIN / 10;

    while (*str >= '0' && *str <= '9') {
        int digit = *str - '0';

        if (negative) {
            // Check for overflow before multiplication
            if (value < INT_MIN_DIV_10) {
                return false;
            }
            value *= 10;
            // Check for overflow before subtraction
            if (value < INT_MIN + digit) {
                return false;
            }
            value -= digit;
        } else {
            // Check for overflow before multiplication
            if (value > INT_MAX_DIV_10) {
                return false;
            }
            value *= 10;
            // Check for overflow before addition
            if (value > INT_MAX - digit) {
                return false;
            }
            value += digit;
        }
        str++;
    }

    *result = value;
    return true;
}

} //end namespace MicroOcpp
