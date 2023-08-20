// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CONFIGURATIONCONTAINER_H
#define CONFIGURATIONCONTAINER_H

#include <vector>
#include <memory>

#include <ArduinoOcpp/Core/ConfigurationKeyValue.h>

namespace ArduinoOcpp {

class ConfigurationContainer {
private:
    std::vector<uint16_t> configurations_revision;
    const char *filename;

protected:
    std::vector<std::shared_ptr<AbstractConfiguration>> configurations;

    ConfigurationContainer(const char *filename) : filename(filename) { }

    //Checks if configurations_revision is equal to (for all) configurations->getValueRevision(). If not, it refreshes the record
    bool configurationsUpdated();
public:
    virtual ~ConfigurationContainer() = default;

    virtual bool load() = 0;

    virtual bool save() = 0;

    const char *getFilename() {return filename;};

    std::shared_ptr<AbstractConfiguration> getConfiguration(const char *key);
    std::vector<std::shared_ptr<AbstractConfiguration>>::iterator configurationsIteratorBegin() {return configurations.begin();}
    std::vector<std::shared_ptr<AbstractConfiguration>>::iterator configurationsIteratorEnd() {return configurations.end();}
    bool removeConfiguration(std::shared_ptr<AbstractConfiguration> configuration);
    void addConfiguration(std::shared_ptr<AbstractConfiguration> configuration);
};

class ConfigurationContainerVolatile : public ConfigurationContainer {
public:
    ConfigurationContainerVolatile(const char *filename) : ConfigurationContainer(filename) { }

    bool load() {return true;}

    bool save() {return true;}
};

} //end namespace ArduinoOcpp

#endif
