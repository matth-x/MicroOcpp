// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CONFIGURATIONCONTAINERFLASH_H
#define CONFIGURATIONCONTAINERFLASH_H

#include <MicroOcpp/Core/ConfigurationContainer.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>

namespace MicroOcpp {

class ConfigurationContainerFlash : public ConfigurationContainer {
    std::shared_ptr<FilesystemAdapter> filesystem;
public:
    ConfigurationContainerFlash(std::shared_ptr<FilesystemAdapter> filesystem, const char *filename) :
            ConfigurationContainer(filename), filesystem(filesystem) { }

    ~ConfigurationContainerFlash() = default;

    bool load();

    bool save();

};

} //end namespace MicroOcpp

#endif
