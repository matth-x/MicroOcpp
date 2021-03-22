// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <ArduinoJson.h>
#include <LinkedList.h>
#include <memory>

class AbstractConfiguration {
private:
    char *key = NULL;
    size_t key_size = 0; // key=NULL --> key_size = 0; key = "" --> key_size = 1; key = "A" --> key_size = 2
    bool rebootRequiredWhenChanged = false;
    bool toBeRemovedFlag = false;
    bool writepermission = true; //write permission for ChangeConfiguration operation
protected:
    uint16_t value_revision = 0; //number of changes of subclass-member "value". This will be important for the client to detect if there was a change
    bool initializedValue = false;
    bool dirty = false;

    AbstractConfiguration();
    AbstractConfiguration(JsonObject storedKeyValuePair);
    size_t getStorageHeaderJsonCapacity();
    void storeStorageHeader(JsonObject keyValuePair);
    size_t getOcppMsgHeaderJsonCapacity();
    void storeOcppMsgHeader(JsonObject keyValuePair);
    bool isValid();
public:
    virtual ~AbstractConfiguration();
    bool setKey(const char *key);
    void printKey();
    void revokeWritePermission();
    bool hasWritePermission();
    void requireRebootWhenChanged();
    bool requiresRebootWhenChanged();
    bool toBeRemoved();
    void setToBeRemoved();
    void resetToBeRemovedFlag();
    //idea: commit(); //probably better than global commit(): what if configurations will be split into multiple files in future?

    uint16_t getValueRevision();
    int compareKey(const char *other);

    virtual std::shared_ptr<DynamicJsonDocument> toJsonStorageEntry() = 0;
    virtual std::shared_ptr<DynamicJsonDocument> toJsonOcppMsgEntry() = 0;
};

template <class T>
class Configuration : public AbstractConfiguration {
private:
    T value;
    const char *getSerializedTypeSpecifier(); //returns "int" or "float" as written to the configuration Json file
public:
    Configuration();
    Configuration(JsonObject storedKeyValuePair);
    T operator=(T newVal);
    operator T() { //TODO move to Configuration.cpp
        if (!initializedValue) {
            Serial.print(F("[Configuration<T>] Tried to access value without preceeding initialization: "));
            printKey();
            Serial.println();
        }
        return value;
    }
    bool isValid();
    //operator bool();

    void initializeValue(T init);

    std::shared_ptr<DynamicJsonDocument> toJsonStorageEntry();
    std::shared_ptr<DynamicJsonDocument> toJsonOcppMsgEntry();
};

template <>
class Configuration<const char *> : public AbstractConfiguration {
private:
    char *value = NULL;
    char *valueReadOnlyCopy = NULL;
    size_t value_size = 0;
    const char *getSerializedTypeSpecifier() {return "string";}
public:
    Configuration();
    Configuration(JsonObject storedKeyValuePair);
    ~Configuration();
    bool setValue(const char *newVal, size_t buffsize);
    String &operator=(String &newVal);
    operator const char*();
    //operator String();
    bool isValid();
    //operator bool();
    size_t getBuffsize();

    bool initializeValue(const char *initval, size_t initsize);

    std::shared_ptr<DynamicJsonDocument> toJsonStorageEntry();
    std::shared_ptr<DynamicJsonDocument> toJsonOcppMsgEntry();
};

std::shared_ptr<Configuration<int>> declareConfiguration(const char *key, int defaultValue, bool writePermission = true, bool rebootRequiredWhenChanged = false);
std::shared_ptr<Configuration<float>> declareConfiguration(const char *key, float defaultValue, bool writePermission = true, bool rebootRequiredWhenChanged = false);
std::shared_ptr<Configuration<const char *>> declareConfiguration(const char *key, const char *defaultValue, bool writePermission = true, bool rebootRequiredWhenChanged = false);

std::shared_ptr<Configuration<int>> setConfiguration(const char *key, int value);
std::shared_ptr<Configuration<float>> setConfiguration(const char *key, float value);
std::shared_ptr<Configuration<const char *>> setConfiguration(const char *key, const char *value, size_t buffsize);

std::shared_ptr<Configuration<int>> getConfiguration_Int(const char *key);
std::shared_ptr<Configuration<float>> getConfiguration_Float(const char *key);
std::shared_ptr<Configuration<const char *>> getConfiguration_string(const char *key);

std::shared_ptr<AbstractConfiguration> getConfiguration(const char *key);
std::shared_ptr<LinkedList<std::shared_ptr<AbstractConfiguration>>> getAllConfigurations();

bool configuration_init();
bool configuration_save();

#endif
