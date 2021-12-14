// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef CONFIGURATIONKEYVALUE_H
#define CONFIGURATIONKEYVALUE_H

#include <ArduinoJson.h>
#include <memory>

namespace ArduinoOcpp {

class AbstractConfiguration {
private:
    char *key = nullptr;
    size_t key_size = 0; // key=nullptr --> key_size = 0; key = "" --> key_size = 1; key = "A" --> key_size = 2

    bool rebootRequiredWhenChanged = false;

    bool remotePeerCanWrite = true;
    bool remotePeerCanRead = true;
    bool localClientCanWrite = true;
//    const char *serializedAccessControl();
//    void loadFromSerializedAccessControl(const char *access);

    bool toBeRemovedFlag = false;
protected:
    uint16_t value_revision = 0; //number of changes of subclass-member "value". This will be important for the client to detect if there was a change
    bool initializedValue = false;

    AbstractConfiguration();
    AbstractConfiguration(JsonObject &storedKeyValuePair);
    size_t getStorageHeaderJsonCapacity();
    void storeStorageHeader(JsonObject &keyValuePair);
    size_t getOcppMsgHeaderJsonCapacity();
    void storeOcppMsgHeader(JsonObject &keyValuePair);
    bool isValid();

    bool permissionLocalClientCanWrite() {return localClientCanWrite;}
public:
    virtual ~AbstractConfiguration();
    bool setKey(const char *key);
    void printKey();

    void requireRebootWhenChanged();
    bool requiresRebootWhenChanged();
    bool toBeRemoved();
    void setToBeRemoved();
    void resetToBeRemovedFlag();

    uint16_t getValueRevision();
    bool keyEquals(const char *other);

    virtual std::shared_ptr<DynamicJsonDocument> toJsonStorageEntry() = 0;
    virtual std::shared_ptr<DynamicJsonDocument> toJsonOcppMsgEntry() = 0;

    virtual const char *getSerializedType() = 0;

    bool permissionRemotePeerCanWrite() {return remotePeerCanWrite;}
    bool permissionRemotePeerCanRead() {return remotePeerCanRead;}
    void revokePermissionRemotePeerCanWrite() {remotePeerCanWrite = false;}
    void revokePermissionRemotePeerCanRead() {remotePeerCanRead = false;}
    void revokePermissionLocalClientCanWrite() {localClientCanWrite = false;}
};


template<class T>
struct SerializedType {
    static const char *get() {return "undefined";}
};

template<> struct SerializedType<int> {static const char *get() {return "int";}};
template<> struct SerializedType<float> {static const char *get() {return "float";}};
template<> struct SerializedType<const char*> {static const char *get() {return "string";}};

template <class T>
class Configuration : public AbstractConfiguration {
private:
    T value;
    size_t getValueJsonCapacity();
public:
    Configuration();
    Configuration(JsonObject &storedKeyValuePair);
    const T &operator=(const T & newVal);
    operator T();
    bool isValid();

    std::shared_ptr<DynamicJsonDocument> toJsonStorageEntry();
    std::shared_ptr<DynamicJsonDocument> toJsonOcppMsgEntry();

    const char *getSerializedType() {return SerializedType<T>::get();} //returns "int" or "float" as written to the configuration Json file
};

template <>
class Configuration<const char *> : public AbstractConfiguration {
private:
    char *value = nullptr;
    char *valueReadOnlyCopy = nullptr;
    size_t value_size = 0;
    size_t getValueJsonCapacity();
public:
    Configuration();
    Configuration(JsonObject &storedKeyValuePair);
    ~Configuration();
    bool setValue(const char *newVal, size_t buffsize);
    const char *operator=(const char *newVal);
    operator const char*();
    bool isValid();
    size_t getBuffsize();

    std::shared_ptr<DynamicJsonDocument> toJsonStorageEntry();
    std::shared_ptr<DynamicJsonDocument> toJsonOcppMsgEntry();

    const char *getSerializedType() {return SerializedType<const char *>::get();}
};

} //end namespace ArduinoOcpp

#endif
