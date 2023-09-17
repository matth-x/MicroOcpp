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

namespace ConfigurationLocal {

std::shared_ptr<FilesystemAdapter> filesystem;
std::vector<std::shared_ptr<ConfigurationContainer>> configurationContainers;

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

template<class T>
std::shared_ptr<Configuration> declareConfiguration(const char *key, T factoryDef, const char *filename, bool readonly, bool rebootRequired, bool accessible) {

    auto container = getContainer(filename);
    
    if (!container) {
        MOCPP_DBG_DEBUG("init new configurations container: %s", filename);

        container = createConfigurationContainer(filename, accessible);
        if (!container) {
            MOCPP_DBG_ERR("OOM");
            return nullptr;
        }
        configurationContainers.push_back(container);

        if (!container->load()) {
            MOCPP_DBG_WARN("Cannot load file contents. Path will be overwritten");
            (void)0;
        }
    }

    if (container->isAccessible() != accessible) {
        MOCPP_DBG_ERR("key %s: declared %s but config file is %s", key, accessible ? "accessible" : "inaccessible", container->isAccessible() ? "accessible" : "inaccessible");
        (void)0;
    }

    std::shared_ptr<Configuration> configuration;

    switch (convertType<T>()) {
        case TConfig::Int:
            configuration = container->declareConfigurationInt(key, factoryDef, readonly, rebootRequired);
            break;
        case TConfig::Bool:
            configuration = container->declareConfigurationBool(key, factoryDef, readonly, rebootRequired);
            break;
        case TConfig::String:
            configuration = container->declareConfigurationString(key, factoryDef, readonly, rebootRequired);
            break;
    }

    return configuration;
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
}

void configuration_deinit() {
    configurationContainers.clear();
    filesystem.reset();
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
