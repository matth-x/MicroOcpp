// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/ConfigurationContainerFlash.h>
#include <MicroOcpp/Debug.h>

#include <string.h>
#include <vector>
#include <algorithm>
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
std::vector<std::shared_ptr<ConfigurationContainer>> configurationContainers;
std::vector<Validator> validators;

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
    std::vector<std::shared_ptr<ConfigurationContainer>>::iterator container = std::find_if(configurationContainers.begin(), configurationContainers.end(),
        [filename](std::shared_ptr<ConfigurationContainer> &elem) {
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
        (void)0;
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
                (void)0;
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

std::vector<ConfigurationContainer*> getConfigurationContainersPublic() {
    std::vector<ConfigurationContainer*> res;

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
    configurationContainers.clear();
    validators.clear();
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

} //end namespace MicroOcpp
