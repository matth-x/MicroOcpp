// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CONFIGURATIONKEYVALUE_H
#define CONFIGURATIONKEYVALUE_H

#include <ArduinoJson.h>
#include <memory>
#include <functional>
#include <string>

namespace MicroOcpp {

enum TConfig : uint8_t {
    None,
    Int,
    Float,
    String,
    Bool
};

class Configuration {
private:
    std::function<bool(const char*)> validator;

    bool rebootRequired = false;
    bool writePermission = true;
    bool readPermission = true;
protected:
    uint16_t value_revision = 0;
public:

    virtual const char *getKey() = 0;

    virtual void setInt(int) = 0;
    virtual void setBool(bool) = 0;
    virtual bool setString(const char*) = 0;

    virtual int getInt() = 0;
    virtual bool getBool() = 0;
    virtual const char *getString() = 0;

    virtual TConfig getType() = 0;

    uint16_t getValueRevision(); 

    void setRequiresReboot();
    bool getRequiresReboot();

    void revokeWritePermission();
    bool getWritePermission();

    void revokeReadPermission();
    bool getReadPermission();

    void setValidator(std::function<bool(const char*)> validator);
    std::function<bool(const char*)> getValidator();
};

template<class K, class T>
class ConfigConcrete : public Configuration {
private:
    K key;
    T val;
public:

    ConfigConcrete();

    ~ConfigConcrete() {
        ConfigHelpers::deinitKey(key);
    }

    bool setKey(const char *key);

    const char *getKey() override;

    int getInt() override;

    bool getBool() override;

    const char *getString() override;

    void setInt(int v) override;

    void setBool(bool v) override;

    bool setString(const char *v) override;

    TConfig getType() override;
};

} //end namespace MicroOcpp

#endif
