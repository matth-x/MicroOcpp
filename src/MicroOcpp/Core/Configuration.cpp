// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Debug.h>

#include <string.h>
#include <vector>
#include <algorithm>
#include <ArduinoJson.h>

namespace ArduinoOcpp {

std::shared_ptr<FilesystemAdapter> filesystem;

template<class T>
std::shared_ptr<Configuration<T>> createConfiguration(const char *key, T value) {

    if (!key || !*key) {
        AO_DBG_ERR("invalid args");
        return nullptr;
    }

    std::shared_ptr<Configuration<T>> configuration = std::make_shared<Configuration<T>>(key, value);

    return configuration;
}

std::shared_ptr<Configuration<const char *>> createConfiguration(const char *key, const char *value) {

    if (!key || !*key || !value) {
        AO_DBG_ERR("invalid args");
        return nullptr;
    }

    std::shared_ptr<Configuration<const char*>> configuration = std::make_shared<Configuration<const char*>>(key, value);

    return configuration;
}

std::unique_ptr<ConfigurationContainer> createConfigurationContainer(const char *filename) {
    //create non-persistent Configuration store (i.e. lives only in RAM) if
    //     - Flash FS usage is switched off OR
    //     - Filename starts with "/volatile"
    if (!filesystem ||
                 !strncmp(filename, CONFIGURATION_VOLATILE, strlen(CONFIGURATION_VOLATILE))) {
        return std::unique_ptr<ConfigurationContainer>(new ConfigurationContainerVolatile(filename));
    } else {
        //create persistent Configuration store. This is the normal caseS
        return std::unique_ptr<ConfigurationContainer>(new ConfigurationContainerFlash(filesystem, filename));
    }
}

std::vector<std::shared_ptr<ConfigurationContainer>> configurationContainers;

void addConfigurationContainer(std::shared_ptr<ConfigurationContainer> container) {
    configurationContainers.push_back(container);
}

std::vector<std::shared_ptr<ConfigurationContainer>>::iterator getConfigurationContainersBegin() {
    return configurationContainers.begin();
}

std::vector<std::shared_ptr<ConfigurationContainer>>::iterator getConfigurationContainersEnd() {
    return configurationContainers.end();
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
std::shared_ptr<Configuration<T>> declareConfiguration(const char *key, T defaultValue, const char *filename, bool remotePeerCanWrite, bool remotePeerCanRead, bool localClientCanWrite, bool rebootRequiredWhenChanged) {
    //already existent? --> stored in last session --> do set default content, but set writepermission flag
    
    std::shared_ptr<ConfigurationContainer> container = getContainer(filename);
    
    if (!container) {
        AO_DBG_DEBUG("init new configurations container: %s", filename);

        container = createConfigurationContainer(filename);
        configurationContainers.push_back(container);

        if (!container->load()) {
            AO_DBG_WARN("Cannot load file contents. Path will be overwritten");
        }
    }

    std::shared_ptr<AbstractConfiguration> configuration = container->getConfiguration(key);

    if (configuration && strcmp(configuration->getSerializedType(), SerializedType<T>::get())) {
        AO_DBG_ERR("conflicting declared types for %s. Discard old config", key);
        container->removeConfiguration(configuration);
        configuration = nullptr;
    }

    std::shared_ptr<Configuration<T>> configurationConcrete = std::static_pointer_cast<Configuration<T>>(configuration);

    if (!configurationConcrete) {
        configurationConcrete = createConfiguration<T>(key, defaultValue);
        configuration = std::static_pointer_cast<AbstractConfiguration>(configurationConcrete);

        if (!configuration) {
            AO_DBG_ERR("Cannot find configuration stored from previous session and cannot create new one! Abort");
            return nullptr;
        }
        container->addConfiguration(configuration);
    }

    if (!remotePeerCanWrite)
        configuration->revokePermissionRemotePeerCanWrite();
    if (!remotePeerCanRead)
        configuration->revokePermissionRemotePeerCanRead();
    if (!localClientCanWrite)
        configuration->revokePermissionLocalClientCanWrite();
    if (rebootRequiredWhenChanged)
        configuration->requireRebootWhenChanged();

    return configurationConcrete;
}

namespace Ocpp16 {

std::shared_ptr<AbstractConfiguration> getConfiguration(const char *key) {
    std::shared_ptr<AbstractConfiguration> result = nullptr;

    for (auto container = configurationContainers.begin(); container != configurationContainers.end(); container++) {
        result = (*container)->getConfiguration(key);
        if (result)
            return result;
    }
    return nullptr;
}

std::unique_ptr<std::vector<std::shared_ptr<AbstractConfiguration>>> getAllConfigurations() { //TODO maybe change to iterator?
    auto result = std::unique_ptr<std::vector<std::shared_ptr<AbstractConfiguration>>>(
                              new std::vector<std::shared_ptr<AbstractConfiguration>>()
    );

    for (auto container = configurationContainers.begin(); container != configurationContainers.end(); container++) {
        for (auto config = (*container)->configurationsIteratorBegin(); config != (*container)->configurationsIteratorEnd(); config++) {
            if ((*config)->permissionRemotePeerCanRead()) {
                result->push_back(*config);
            }
        }
    }

    return result;
}

} //end namespace Ocpp16

bool configuration_inited = false;

bool configuration_init(std::shared_ptr<FilesystemAdapter> _filesystem) {
    if (configuration_inited)
        return true; //configuration_init() already called; tolerate multiple calls so user can use this store for
                     //credentials outside ArduinoOcpp which need to be loaded before ocpp_initialize()
    
    filesystem = _filesystem;

    if (!filesystem) {
        configuration_inited = true;
        return true; //no filesystem, nothing can go wrong
    }

    std::shared_ptr<ConfigurationContainer> containerDefault = nullptr;
    for (auto container = configurationContainers.begin(); container != configurationContainers.end(); container++) {
        if (!strcmp((*container)->getFilename(), CONFIGURATION_FN)) {
            containerDefault = (*container);
            break;
        }
    }

    bool success = true;

    if (containerDefault) {
        AO_DBG_DEBUG("Found default container before calling configuration_init(). If you added");
        AO_DBG_DEBUG(" > the container manually, please ensure to call load(). If not, it is a hint");
        AO_DBG_DEBUG(" > that declareConfiguration() was called too early");
        (void)0;
    } else {
        containerDefault = createConfigurationContainer(CONFIGURATION_FN);
        if (!containerDefault->load()) {
            AO_DBG_ERR("Loading default configurations file failed");
            success = false;
        }
        configurationContainers.push_back(containerDefault);
    }

    configuration_inited = success;
    return success;
}

void configuration_deinit() {
    configurationContainers.clear();
    filesystem.reset();
    configuration_inited = false;
}

bool configuration_save() {
    bool success = true;

    for (auto container = configurationContainers.begin(); container != configurationContainers.end(); container++) {
        if (!(*container)->save()) {
            success = false;
        }
    }

    return success;
}

template std::shared_ptr<Configuration<int>> createConfiguration(const char *key, int value);
template std::shared_ptr<Configuration<float>> createConfiguration(const char *key, float value);
template std::shared_ptr<Configuration<bool>> createConfiguration(const char *key, bool value);
template std::shared_ptr<Configuration<const char *>> createConfiguration(const char *key, const char * value);

template std::shared_ptr<Configuration<int>> declareConfiguration(const char *key, int defaultValue, const char *filename, bool remotePeerCanWrite, bool remotePeerCanRead, bool localClientCanWrite, bool rebootRequiredWhenChanged);
template std::shared_ptr<Configuration<float>> declareConfiguration(const char *key, float defaultValue, const char *filename, bool remotePeerCanWrite, bool remotePeerCanRead, bool localClientCanWrite, bool rebootRequiredWhenChanged);
template std::shared_ptr<Configuration<bool>> declareConfiguration(const char *key, bool defaultValue, const char *filename, bool remotePeerCanWrite, bool remotePeerCanRead, bool localClientCanWrite, bool rebootRequiredWhenChanged);
template std::shared_ptr<Configuration<const char *>> declareConfiguration(const char *key, const char *defaultValue, const char *filename, bool remotePeerCanWrite, bool remotePeerCanRead, bool localClientCanWrite, bool rebootRequiredWhenChanged);

} //end namespace ArduinoOcpp
