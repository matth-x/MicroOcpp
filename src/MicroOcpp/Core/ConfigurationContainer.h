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

    virtual ~ConfigurationContainer();

    const char *getFilename() {return filename;}
    bool isAccessible() {return accessible;}

    virtual bool load() = 0; //called at the end of mocpp_intialize, to load the configurations with the stored value
    virtual bool save() = 0;

    virtual std::shared_ptr<Configuration> createConfiguration(TConfig type, const char *key) = 0;
    virtual void remove(Configuration *config) = 0;

    virtual size_t size() = 0;
    virtual Configuration *getConfiguration(size_t i) = 0;
    virtual std::shared_ptr<Configuration> getConfiguration(const char *key) = 0;

    virtual void loadStaticKey(Configuration& config, const char *key) { } //possible optimization: can replace internal key with passed static key
};

class ConfigurationContainerVolatile : public ConfigurationContainer {
private:
    std::vector<std::shared_ptr<Configuration>> configurations;
public:
    ConfigurationContainerVolatile(const char *filename, bool accessible);

    //ConfigurationContainer definitions
    bool load() override;
    bool save() override;
    std::shared_ptr<Configuration> createConfiguration(TConfig type, const char *key) override;
    void remove(Configuration *config) override;
    size_t size() override;
    Configuration *getConfiguration(size_t i) override;
    std::shared_ptr<Configuration> getConfiguration(const char *key) override;

    //add custom Configuration object
    void add(std::shared_ptr<Configuration> c);
};

std::unique_ptr<ConfigurationContainerVolatile> makeConfigurationContainerVolatile(const char *filename, bool accessible);

} //end namespace MicroOcpp

#endif
