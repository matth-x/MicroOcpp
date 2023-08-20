// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <MicroOcpp/Core/ConfigurationKeyValue.h>
#include <MicroOcpp/Core/ConfigurationOptions.h>
#include <MicroOcpp/Core/ConfigurationContainerFlash.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>

#include <memory>
#include <vector>

#define CONFIGURATION_FN (MOCPP_FILENAME_PREFIX "ocpp-config.jsn")
#define CONFIGURATION_VOLATILE "/volatile"
#define MOCPP_KEYVALUE_FN (MOCPP_FILENAME_PREFIX "client-state.jsn")

namespace MicroOcpp {

template <class T>
std::shared_ptr<Configuration<T>> declareConfiguration(const char *key, T defaultValue, const char *filename = CONFIGURATION_FN, bool remotePeerCanWrite = true, bool remotePeerCanRead = true, bool localClientCanWrite = true, bool rebootRequiredWhenChanged = false);

void addConfigurationContainer(std::shared_ptr<ConfigurationContainer> container);
std::vector<std::shared_ptr<ConfigurationContainer>>::iterator getConfigurationContainersBegin();
std::vector<std::shared_ptr<ConfigurationContainer>>::iterator getConfigurationContainersEnd();


namespace Ocpp16 {

    std::shared_ptr<AbstractConfiguration> getConfiguration(const char *key);
    std::unique_ptr<std::vector<std::shared_ptr<AbstractConfiguration>>> getAllConfigurations();
}

bool configuration_init(std::shared_ptr<FilesystemAdapter> filesytem);
void configuration_deinit();

bool configuration_save();

} //end namespace MicroOcpp
#endif
