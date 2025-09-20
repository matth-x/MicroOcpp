// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

/*
 * Implementation of the UCs B05 - B06
 */

#ifndef MO_VARIABLE_H
#define MO_VARIABLE_H

#include <stdint.h>

#ifdef __cplusplus
#include <memory>
#include <limits>
#endif //__cplusplus

#include <MicroOcpp/Model/Common/EvseId.h>
#include <MicroOcpp/Model/Common/Mutability.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#ifndef MO_VARIABLE_TYPECHECK
#define MO_VARIABLE_TYPECHECK 1
#endif

#ifdef __cplusplus

namespace MicroOcpp {

namespace v201 {

// VariableCharacteristicsType (2.51)
struct VariableCharacteristics : public MemoryManaged {

    // DataEnumType (3.26)
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

    VariableCharacteristics() : MemoryManaged("v201.Variables.VariableCharacteristics") { }
};

// SetVariableStatusEnumType (3.79)
enum class SetVariableStatus : uint8_t {
    Accepted,
    Rejected,
    UnknownComponent,
    UnknownVariable,
    NotSupportedAttributeType,
    RebootRequired
};

// GetVariableStatusEnumType (3.41)
enum class GetVariableStatus : uint8_t {
    Accepted,
    Rejected,
    UnknownComponent,
    UnknownVariable,
    NotSupportedAttributeType
};

// ReportBaseEnumType (3.70)
typedef enum ReportBase {
    ReportBase_ConfigurationInventory,
    ReportBase_FullInventory,
    ReportBase_SummaryInventory
}   ReportBase;

// GenericDeviceModelStatus (3.34)
typedef enum GenericDeviceModelStatus {
    GenericDeviceModelStatus_Accepted,
    GenericDeviceModelStatus_Rejected,
    GenericDeviceModelStatus_NotSupported,
    GenericDeviceModelStatus_EmptyResultSet
}   GenericDeviceModelStatus;

// VariableMonitoringType (2.52)
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
    VariableMonitor(int id, bool transaction, float value, Type type, int severity) :
            id(id), transaction(transaction), value(value), type(type), severity(severity) { }
};

// ComponentType (2.16)
struct ComponentId {
    const char *name; // zero copy
    //const char *instance; // not supported in this implementation
    EvseId evse {-1};

    ComponentId(const char *name = nullptr);
    ComponentId(const char *name, EvseId evse);

    bool equals(const ComponentId& other) const;
};

/*
 * Corresponds to VariableType (2.53)
 *
 * Template method pattern: this is a super-class which has hook-methods for storing and fetching
 * the value of the variable. To make it use the host system's key-value store, extend this class
 * with a custom implementation of the virtual methods and pass its instances to MO.
 */
class Variable : public MemoryManaged {
public:
    //AttributeEnumType (3.2)
    enum class AttributeType : uint8_t {
        Actual,
        Target,
        MinSet,
        MaxSet
    };

    struct AttributeTypeSet {
        uint8_t flag = 0;

        bool has(Variable::AttributeType type);
        AttributeTypeSet& set(Variable::AttributeType type);
        size_t count();

        AttributeTypeSet(AttributeType attrType = AttributeType::Actual);
    };

    //MO-internal optimization: if value is only in int range, store it in more compact representation
    enum class InternalDataType : uint8_t {
        Int,
        Bool,
        String
    };
private:
    const char *variableName = nullptr;
    ComponentId component;

    // VariableCharacteristicsType (2.51)
    std::unique_ptr<VariableCharacteristics> characteristics; //optional VariableCharacteristics
    VariableCharacteristics::DataType dataType; //mandatory
    bool supportsMonitoring = false; //mandatory
    bool rebootRequired = false; //MO-internal: if to respond status RebootRequired on SetVariables

    // VariableAttributeType (2.50)
    Mutability mutability = Mutability::ReadWrite;
    bool persistent = false;
    bool constant = false;

    AttributeTypeSet attributes;

    // VariableMonitoringType (2.52)
    //std::vector<VariableMonitor> monitors; // uncomment when testing Monitors
public:
    Variable(AttributeTypeSet attributes);

    virtual ~Variable();

    void setName(const char *name); //zero-copy
    const char *getName() const;

    void setComponentId(const ComponentId& componentId); //zero-copy
    const ComponentId& getComponentId() const;

    // set Value of Variable
    virtual void setInt(int val, AttributeType attrType = AttributeType::Actual);
    virtual void setBool(bool val, AttributeType attrType = AttributeType::Actual);
    virtual bool setString(const char *val, AttributeType attrType = AttributeType::Actual);

    // get Value of Variable
    virtual int getInt(AttributeType attrType = AttributeType::Actual);
    virtual bool getBool(AttributeType attrType = AttributeType::Actual);
    virtual const char *getString(AttributeType attrType = AttributeType::Actual); //always returns c-string (empty if undefined)

    virtual InternalDataType getInternalDataType() = 0; //corresponds to MO internal value representation
    bool hasAttribute(AttributeType attrType);

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

    virtual uint16_t getWriteCount() = 0; //get write count (use this as a pre-check if the value changed)
};

std::unique_ptr<Variable> makeVariable(Variable::InternalDataType dtype, Variable::AttributeTypeSet supportAttributes);

} //namespace v201
} //namespace MicroOcpp
#endif //__cplusplus
#endif //MO_ENABLE_V201
#endif
