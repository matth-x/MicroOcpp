// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CONFIGURATIONKEYVALUE_H
#define CONFIGURATIONKEYVALUE_H

#include <ArduinoJson.h>
#include <memory>

#define MOCPP_CONFIG_MAX_VALSTRSIZE 128

namespace MicroOcpp {

using revision_t = uint16_t;

enum TConfig : uint8_t {
    Int,
    Bool,
    String
};

template<class T>
TConfig convertType();

class Configuration {
protected:
    revision_t value_revision = 0; //write access counter; used to check if this config has been changed
private:
    bool rebootRequired = false;
    bool readOnly = false;
public:
    virtual ~Configuration();

    virtual bool setKey(const char *key) = 0;
    virtual const char *getKey() = 0;

    virtual void setInt(int);
    virtual void setBool(bool);
    virtual bool setString(const char*);

    virtual int getInt();
    virtual bool getBool();
    virtual const char *getString();

    virtual TConfig getType() = 0;

    revision_t getValueRevision(); 

    void setRebootRequired();
    bool isRebootRequired();

    void setReadOnly();
    bool isReadOnly();
};

/*
 * Default implementations of the Configuration interface.
 *
 * How to use custom implementations: for each OCPP config, pass a config instance to the OCPP lib
 * before its initialization stage. Then the library won't create new config objects but 
 */

template<class T>
std::unique_ptr<Configuration> makeConfig(const char *key, T val);

const char *serializeTConfig(TConfig type);
bool deserializeTConfig(const char *serialized, TConfig& out);

} //end namespace MicroOcpp

#endif
