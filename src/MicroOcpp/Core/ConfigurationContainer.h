// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CONFIGURATIONCONTAINER_H
#define CONFIGURATIONCONTAINER_H

#include <vector>
#include <memory>

#include <MicroOcpp/Core/ConfigurationKeyValue.h>

namespace MicroOcpp {

class ConfigurationContainer {
private:
    const char *filename;
    bool accessible;
public:
    ConfigurationContainer(const char *filename, bool accessible) : filename(filename), accessible(accessible) { }

    const char *getFilename() {return filename;}
    bool isAccessible() {return accessible;}

    virtual bool load() = 0; //called at the end of mocpp_intialize, to load the configurations with the stored value
    virtual bool save() = 0;

    virtual std::shared_ptr<Configuration> declareConfigurationInt(const char *key, int factoryDef, bool readonly = false, bool rebootRequired = false) = 0;
    virtual std::shared_ptr<Configuration> declareConfigurationBool(const char *key, bool factoryDef, bool readonly = false, bool rebootRequired = false) = 0;
    virtual std::shared_ptr<Configuration> declareConfigurationString(const char *key, const char *factoryDef, bool readonly = false, bool rebootRequired = false) = 0;
    
    virtual size_t getConfigurationCount() = 0;
    virtual Configuration *getConfiguration(size_t i) = 0;
    virtual Configuration *getConfiguration(const char *key) = 0;
};

//Utils class only used for internal ConfigurationContainer implementations
class ConfigurationPool {
private:
    std::vector<std::shared_ptr<Configuration>> configurations;

    std::shared_ptr<Configuration> getConfiguration_typesafe(const char *key, TConfig type);

public:
    template<class T>
    std::shared_ptr<Configuration> declareConfiguration(const char *key, T factoryDef, bool readonly = false, bool rebootRequired = false);
    
    std::vector<std::shared_ptr<Configuration>>& getConfigurations();
    
    size_t getConfigurationCount() const;

    Configuration *getConfiguration(size_t i) const;

    Configuration *getConfiguration(const char *key) const;
};

template<> std::shared_ptr<Configuration> ConfigurationPool::declareConfiguration<int>(const char *key, int factoryDef, bool readonly, bool rebootRequired);
template<> std::shared_ptr<Configuration> ConfigurationPool::declareConfiguration<bool>(const char *key, bool factoryDef, bool readonly, bool rebootRequired);
template<> std::shared_ptr<Configuration> ConfigurationPool::declareConfiguration<const char*>(const char *key, const char *factoryDef, bool readonly, bool rebootRequired);

std::unique_ptr<ConfigurationContainer> makeConfigurationContainerVolatile(const char *filename, bool accessible);

} //end namespace MicroOcpp

#endif
