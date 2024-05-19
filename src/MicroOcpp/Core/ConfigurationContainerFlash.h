// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CONFIGURATIONCONTAINERFLASH_H
#define MO_CONFIGURATIONCONTAINERFLASH_H

#include <MicroOcpp/Core/ConfigurationContainer.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>

namespace MicroOcpp {

std::unique_ptr<ConfigurationContainer> makeConfigurationContainerFlash(std::shared_ptr<FilesystemAdapter> filesystem, const char *filename, bool accessible);

} //end namespace MicroOcpp

#endif
