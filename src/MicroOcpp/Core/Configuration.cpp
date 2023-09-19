// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Core/Configuration.h>
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
        MOCPP_DBG_DEBUG("init new configurations container: %s", filename);

        container = createConfigurationContainer(filename, accessible);
        if (!container) {
            MOCPP_DBG_ERR("OOM");
            return nullptr;
        }
        configurationContainers.push_back(container);
    }

    if (container->isAccessible() != accessible) {
        MOCPP_DBG_ERR("%s: conflicting accessibility declarations (expect %s)", filename, container->isAccessible() ? "accessible" : "inaccessible");
        (void)0;
    }

    return container.get();
}

template<>
std::shared_ptr<Configuration> declareConfiguration<int>(const char *key, int factoryDef, const char *filename, bool readonly, bool rebootRequired, bool accessible) {
    if (auto container = declareContainer(filename, accessible)) {
        return container->declareConfigurationInt(key, factoryDef, readonly, rebootRequired);
    }
    return nullptr;
}

template<>
std::shared_ptr<Configuration> declareConfiguration<bool>(const char *key, bool factoryDef, const char *filename, bool readonly, bool rebootRequired, bool accessible) {
    if (auto container = declareContainer(filename, accessible)) {
        return container->declareConfigurationBool(key, factoryDef, readonly, rebootRequired);
    }
    return nullptr;
}

template<>
std::shared_ptr<Configuration> declareConfiguration<const char*>(const char *key, const char *factoryDef, const char *filename, bool readonly, bool rebootRequired, bool accessible) {
    if (auto container = declareContainer(filename, accessible)) {
        return container->declareConfigurationString(key, factoryDef, readonly, rebootRequired);
    }
    return nullptr;
}

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
                return res;
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

bool configuration_load() {
    bool success = true;

    for (auto& container : configurationContainers) {
        if (!container->load()) {
            success = false;
        }
    }

    return success;
}

bool configuration_load(const char *filename) {
    bool success = true;

    for (auto& container : configurationContainers) {
        if (!strcmp(filename, container->getFilename()) && !container->load()) {
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

template std::shared_ptr<Configuration> declareConfiguration<int>(const char *key, int factoryDef, const char *filename, bool readonly, bool rebootRequired, bool accessible);
template std::shared_ptr<Configuration> declareConfiguration<bool>(const char *key, bool factoryDef, const char *filename, bool readonly, bool rebootRequired, bool accessible);
template std::shared_ptr<Configuration> declareConfiguration<const char*>(const char *key, const char *factoryDef, const char *filename, bool readonly, bool rebootRequired, bool accessible);

} //end namespace MicroOcpp
