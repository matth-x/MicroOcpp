// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

/*
 * Implementation of the UCs B05 - B06
 */

#ifndef MO_VARIABLE_H
#define MO_VARIABLE_H

#include <stdint.h>
#include <vector>
#include <memory>
#include <limits>

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

namespace MicroOcpp {

//VariableCharacteristicsType (2.51)
struct VariableCharacteristics {

    //DataEnumType (3.26)
    enum class DataType : uint8_t {
        string,
        decimal,
        integer,
        dateTime,
        boolean,
        OptionList,
        SequenceList,
        MemberList
    };

    const char *unit = nullptr; //no copy
    //DataType dataType; //stored in Variable
    int minLimit = std::numeric_limits<int>::min();
    int maxLimit = std::numeric_limits<int>::max();
    const char *valuesList = nullptr; //no copy
    //bool supportsMonitoring; //stored in Variable
};

/*
 * Corresponds to VariableMonitoringType (2.52)
 */
class VariableMonitor {
public:
    //MonitorEnumType (3.55)
    enum class Type {
        UpperThreshold,
        LowerThreshold,
        Delta,
        Periodic,
        PeriodicClockAligned
    };
private:
    int id;
    bool transaction;
    float value;
    Type type;
    int severity;
public:
    VariableMonitor() = delete;
    VariableMonitor(int id, bool transaction, float value, Type type, int severity);
};

/*
 * Corresponds to VariableType (2.53)
 *
 * Template method pattern: this is a super-class which has hook-methods for storing and fetching
 * the value of the variable. To make it use the host system's key-value store, extend this class
 * with a custom implementation of the virtual methods and pass its instances to MO.
 */
class Variable {
public:
    //AttributeEnumType (3.2)
    enum class AttributeType : uint8_t {
        Actual,
        Target,
        MinSet,
        MaxSet
    };

    //MutabilityEnumType (3.58)
    enum class Mutability : uint8_t {
        ReadOnly,
        WriteOnly,
        ReadWrite
    };

    //MO-internal optimization: if value is only in int range, store it in more compact representation
    enum class InternalDataType : uint8_t {
        Int,
        Bool,
        String
    };
private:
    const char *variableName = nullptr;
    //const char *instance = nullptr; //<-- instance not supported in this implementation
    const char *componentName = nullptr;

    // VariableCharacteristicsType (2.51)
    std::unique_ptr<VariableCharacteristics> characteristics; //optional VariableCharacteristics
    VariableCharacteristics::DataType dataType; //mandatory
    bool supportsMonitoring = false; //mandatory
    bool rebootRequired = false; //MO-internal: if to respond status RebootRequired on SetVariables

    // VariableAttributeType (2.50)
    Mutability mutability = Mutability::ReadWrite;
    bool persistent = false;
    bool constant = false;

    //VariableMonitoring
    //std::vector<VariableMonitor> monitors;
protected:
    uint16_t writeCount = 0; //write access counter; used to check if this config has been changed
public:

    void setName(const char *key); //zero-copy
    const char *getName() const;

    void setComponentId(const char *componentId); //zero-copy
    const char *getComponentId() const;

    // set Value of Variable
    virtual void setInt(int val, AttributeType attrType = AttributeType::Actual);
    virtual void setBool(bool val, AttributeType attrType = AttributeType::Actual);
    virtual bool setString(const char *val, AttributeType attrType = AttributeType::Actual);

    // get Value of Variable
    virtual int getInt(AttributeType attrType = AttributeType::Actual);
    virtual bool getBool(AttributeType attrType = AttributeType::Actual);
    virtual const char *getString(AttributeType attrType = AttributeType::Actual); //always returns c-string (empty if undefined)

    virtual InternalDataType getInternalDataType() = 0; //corresponds to MO internal value representation

    void setVariableDataType(VariableCharacteristics::DataType dataType); //corresponds to OCPP DataEnumType (3.26)
    VariableCharacteristics::DataType getVariableDataType(); //corresponds to OCPP DataEnumType (3.26)
    bool getSupportsMonitoring();
    void setSupportsMonitoring();
    bool isRebootRequired();
    void setRebootRequired();

    void setMutability(Mutability m);
    Mutability getMutability();

    void setPersistent();
    bool isPersistent();

    void setConstant();
    bool isConstant();

    //bool addMonitor(int id, bool transaction, float value, VariableMonitor::Type type, int severity);
    
    uint16_t getWriteCount(); //get write count (use this as a pre-check if the value changed)
};

} // namespace MicroOcpp

#endif // MO_ENABLE_V201
#endif
