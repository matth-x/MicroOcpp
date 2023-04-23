// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <ArduinoOcpp/Core/ConfigurationKeyValue.h>
#include <ArduinoOcpp/Core/ConfigurationOptions.h>
#include <ArduinoOcpp/Core/ConfigurationContainerFlash.h>
#include <ArduinoOcpp/Core/FilesystemAdapter.h>

#include <memory>
#include <vector>

#define CONFIGURATION_FN (AO_FILENAME_PREFIX "arduino-ocpp.cnf")
#define CONFIGURATION_VOLATILE "/volatile"
#define AO_KEYVALUE_FN (AO_FILENAME_PREFIX "client-state.cnf")

namespace ArduinoOcpp {

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
bool configuration_save();

} //end namespace ArduinoOcpp
#endif
