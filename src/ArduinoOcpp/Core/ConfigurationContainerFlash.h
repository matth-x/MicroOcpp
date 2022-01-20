// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef CONFIGURATIONCONTAINERFLASH_H
#define CONFIGURATIONCONTAINERFLASH_H

#include <ArduinoOcpp/Core/ConfigurationContainer.h>

namespace ArduinoOcpp {

class ConfigurationContainerFlash : public ConfigurationContainer {


public:
    ConfigurationContainerFlash(const char *filename) : ConfigurationContainer(filename) { }

    ~ConfigurationContainerFlash() = default;

    bool load();

    bool save();

};

} //end namespace ArduinoOcpp

#endif
