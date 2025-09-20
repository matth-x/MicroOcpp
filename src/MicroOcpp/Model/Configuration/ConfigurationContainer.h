// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_CONFIGURATIONCONTAINER_H
#define MO_CONFIGURATIONCONTAINER_H

#ifdef __cplusplus
#include <memory>
#endif //__cplusplus

#include <MicroOcpp/Model/Configuration/Configuration.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Memory.h>

#if MO_ENABLE_V16

#define MO_CONFIGURATION_FN "ocpp-config.jsn"
#define MO_CONFIGURATION_VOLATILE "/volatile"
#define MO_KEYVALUE_FN "client-state.jsn"

#ifdef __cplusplus
namespace MicroOcpp {
namespace v16 {

class ConfigurationContainer {
public:
    virtual ~ConfigurationContainer();
    virtual size_t size() = 0;
    virtual Configuration *getConfiguration(size_t i) = 0;
    virtual Configuration *getConfiguration(const char *key) = 0;

    virtual bool commit();
};

class ConfigurationContainerNonOwning : public ConfigurationContainer, public MemoryManaged {
private:
    Vector<Configuration*> configurations;
public:
    ConfigurationContainerNonOwning();

    size_t size() override;
    Configuration *getConfiguration(size_t i) override;
    Configuration *getConfiguration(const char *key) override;

    bool add(Configuration *configuration);
};

class ConfigurationContainerOwning : public ConfigurationContainer, public MemoryManaged {
private:
    MO_FilesystemAdapter *filesystem = nullptr;
    char *filename = nullptr;

    Vector<Configuration*> configurations;

    uint16_t trackWriteCount = 0;
    bool checkWriteCountUpdated();

    bool loaded = false;

public:
    ConfigurationContainerOwning();
    ~ConfigurationContainerOwning();

    size_t size() override;
    Configuration *getConfiguration(size_t i) override;
    Configuration *getConfiguration(const char *key) override;

    bool add(std::unique_ptr<Configuration> configuration);

    void setFilesystem(MO_FilesystemAdapter *filesystem);
    bool setFilename(const char *filename);
    const char *getFilename();
    bool load(); //load configurations from flash
    bool commit() override;
};

} //namespace v16
} //namespace MicroOcpp
#endif //__cplusplus
#endif //MO_ENABLE_V16
#endif
