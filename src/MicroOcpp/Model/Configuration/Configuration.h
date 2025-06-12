// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_CONFIGURATION_H
#define MO_CONFIGURATION_H

#include <stdint.h>
#include <memory>

#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Model/Common/Mutability.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16

#ifndef MO_CONFIG_TYPECHECK
#define MO_CONFIG_TYPECHECK 1
#endif

namespace MicroOcpp {

namespace Ocpp16 {

// ConfigurationStatus (7.22)
enum class ConfigurationStatus : uint8_t {
    Accepted,
    Rejected,
    RebootRequired,
    NotSupported
};

/*
 * Single Configuration Key-value pair
 *
 * Template method pattern: this is a super-class which has hook-methods for storing and fetching
 * the value of the configuration. To make it use the host system's key-value store, extend this class
 * with a custom implementation of the virtual methods and pass its instances to MO.
 */
class Configuration : public MemoryManaged {
private:
    const char *key = nullptr;
    bool rebootRequired = false;
    Mutability mutability = Mutability::ReadWrite;
public:
    virtual ~Configuration();

    void setKey(const char *key); //zero-copy
    const char *getKey() const;

    // set Value of Configuration
    virtual void setInt(int val);
    virtual void setBool(bool val);
    virtual bool setString(const char *val);

    // get Value of Configuration
    virtual int getInt();
    virtual bool getBool();
    virtual const char *getString(); //always returns c-string (empty if undefined)

    enum class Type : uint8_t {
        Int,
        Bool,
        String
    };
    virtual Type getType() = 0;

    virtual uint16_t getWriteCount(); //get write count (implement this if MO should persist values on flash)

    void setRebootRequired();
    bool isRebootRequired();

    void setMutability(Mutability mutability); //server-side mutability, i.e. how Get-/ChangeConfiguration can access this config
    Mutability getMutability(); //server-side mutability, i.e. how Get-/ChangeConfiguration can access this config
};

std::unique_ptr<Configuration> makeConfiguration(Configuration::Type dtype);

} //namespace Ocpp16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16
#endif
