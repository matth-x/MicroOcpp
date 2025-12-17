// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CONFIGURATION_H
#define MO_CONFIGURATION_H

#include <MicroOcpp/Core/ConfigurationKeyValue.h>
#include <MicroOcpp/Core/ConfigurationContainer.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Memory.h>

#include <memory>

#define CONFIGURATION_FN (MO_FILENAME_PREFIX "ocpp-config.jsn")
#define CONFIGURATION_VOLATILE "/volatile"
#define MO_KEYVALUE_FN (MO_FILENAME_PREFIX "client-state.jsn")

namespace MicroOcpp {

template <class T>
std::shared_ptr<Configuration> declareConfiguration(const char *key, T factoryDefault, const char *filename = CONFIGURATION_FN, bool readonly = false, bool rebootRequired = false, bool accessible = true);

std::function<bool(const char*)> *getConfigurationValidator(const char *key);
void registerConfigurationValidator(const char *key, std::function<bool(const char*)> validator);

void addConfigurationContainer(std::shared_ptr<ConfigurationContainer> container);

Configuration *getConfigurationPublic(const char *key);
Vector<ConfigurationContainer*> getConfigurationContainersPublic();

bool configuration_init(std::shared_ptr<FilesystemAdapter> filesytem);
void configuration_deinit();

bool configuration_load(const char *filename = nullptr);

bool configuration_save();

bool configuration_clean_unused(); //remove configs which haven't been accessed

//default implementation for common validator
bool VALIDATE_UNSIGNED_INT(const char*);

//safely convert string to int with overflow protection; returns false on null, invalid format, or overflow
bool safeStringToInt(const char *str, int *result);

} //end namespace MicroOcpp
#endif
