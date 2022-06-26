// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef CONFIGURATIONCONTAINERFLASH_H
#define CONFIGURATIONCONTAINERFLASH_H

#include <ArduinoOcpp/Core/ConfigurationContainer.h>
#include <ArduinoOcpp/Core/FilesystemAdapter.h>

namespace ArduinoOcpp {

class ConfigurationContainerFlash : public ConfigurationContainer {
    std::shared_ptr<FilesystemAdapter> filesystem;
public:
    ConfigurationContainerFlash(std::shared_ptr<FilesystemAdapter> filesystem, const char *filename) :
            ConfigurationContainer(filename), filesystem(filesystem) { }

    ~ConfigurationContainerFlash() = default;

    bool load();

    bool save();

};

} //end namespace ArduinoOcpp

#endif
