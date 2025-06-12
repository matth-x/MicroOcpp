// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_CONFIGURATIONSERVICE_H
#define MO_CONFIGURATIONSERVICE_H

#include <MicroOcpp/Model/Configuration/Configuration.h>
#include <MicroOcpp/Model/Configuration/ConfigurationContainer.h>
#include <MicroOcpp/Model/Configuration/ConfigurationDefs.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16

namespace MicroOcpp {

class Context;

namespace Ocpp16 {

template <class T>
struct ConfigurationValidator {
    const char *key;
    void *user_data;
    bool (*validateFn)(T, void*);
    ConfigurationValidator(const char *key, bool (*validate)(T, void*), void *user_data);
    bool validate(T);
};

bool VALIDATE_UNSIGNED_INT(int v, void*); //default implementation for common validator. Returns true if v >= 0

class ConfigurationService : public MemoryManaged {
private:
    Context& context;

    MO_FilesystemAdapter *filesystem = nullptr;

    Vector<ConfigurationContainer*> containers; //does not take ownership
    ConfigurationContainerNonOwning containerExternal;
    Vector<ConfigurationContainerOwning*> containersInternal; //takes ownership
    ConfigurationContainerOwning *getContainerInternal(const char *filename);
    ConfigurationContainerOwning *declareContainer(const char *filename);

    Vector<ConfigurationValidator<int>> validatorInt;
    Vector<ConfigurationValidator<bool>> validatorBool;
    Vector<ConfigurationValidator<const char*>> validatorString;

    ConfigurationValidator<int> *getValidatorInt(const char *key);
    ConfigurationValidator<bool> *getValidatorBool(const char *key);
    ConfigurationValidator<const char*> *getValidatorString(const char *key);

public:
    ConfigurationService(Context& context);
    ~ConfigurationService();

    bool init();

    //Get Configuration. If not existent, create Configuration owned by MO and return
    template <class T>
    Configuration *declareConfiguration(const char *key, T factoryDefault, const char *filename = MO_CONFIGURATION_FN, Mutability mutability = Mutability::ReadWrite, bool rebootRequired = false);

    bool addConfiguration(Configuration *configuration); //Add Configuration without transferring ownership
    bool addConfiguration(std::unique_ptr<Configuration> configuration, const char *filename); //Add Configuration and transfer ownership

    bool addContainer(ConfigurationContainer *container); //Add Container without transferring ownership

    template <class T>
    bool registerValidator(const char *key, bool (*validate)(T, void*), void *userPtr = nullptr);

    bool setup();

    //Get Configuration. If not existent, return nullptr
    Configuration *getConfiguration(const char *key);

    bool getConfiguration(const char **keys, size_t keysSize, Vector<Configuration*>& configurations, Vector<char*>& unknownKeys); //unknownKeys entries are allocated on heap - need to MO_FREE each entry, even after failure
    ConfigurationStatus changeConfiguration(const char *key, const char *value);

    ConfigurationContainer *findContainerOfConfiguration(Configuration *whichConfig);

    bool commit();
};
    
} //namespace Ocpp16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16
#endif
