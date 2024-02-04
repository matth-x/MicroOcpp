// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

/*
 * Implementation of the UCs B05 - B07
 */

#ifndef MO_VARIABLE_H
#define MO_VARIABLE_H

#include <stdint.h>
#include <vector>
#include <memory>

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

namespace MicroOcpp {

/*
 * MO-internal optimization: store data in more compact integer representation if applicable
 */
enum class VariableAttributeType : uint8_t {
    Int,
    Bool,
    String
};

/*
 * Corresponds to VariableAttributeType (2.50)
 *
 * Template method pattern: this is a super-class which has hook-methods for storing and fetching
 * the value of the variable. To make it use the host system's key-value store, extend this class
 * with a custom implementation and pass its instances to MO.
 */
class VariableAttribute {
public:
    //AttributeEnumType (3.2)
    enum class Type : uint8_t {
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

protected:
    uint16_t writeCounter = 0; //write access counter; used to check if this config has been changed
private:
    Type type = Type::Actual;
    Mutability mutability = Mutability::ReadWrite;
    bool persistent = false;
    bool constant = false;

    bool rebootRequired = false;

public:
    virtual ~VariableAttribute();

    virtual void setInt(int);
    virtual void setBool(bool);
    virtual bool setString(const char*);

    virtual int getInt();
    virtual bool getBool();
    virtual const char *getString(); //always returns c-string (empty if undefined)
    
    uint16_t getWriteCounter(); //get write count (use this as a pre-check if the value changed)

    void setType(Type t);
    Type getType();

    void setMutability(Mutability m);
    Mutability getMutability();

    void setPersistent();
    bool isPersistent();

    void setConstant();
    bool isConstant();

    void setRebootRequired();
    bool isRebootRequired();
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
 */
class Variable {
private:
    const char *name = nullptr;
    //const char *instance = nullptr; //<-- instance not supported in this implementation
    const char *componentId = nullptr;
    std::vector<std::unique_ptr<VariableAttribute>> attributes; //attr's are initialized in client code and ownership is transferred to Variable
    std::vector<VariableMonitor> monitors;
public:

    void setName(const char *key); //zero-copy
    const char *getName() const;

    void setComponentId(const char *componentId); //zero-copy
    const char *getComponentId() const;

    bool addAttribute(std::unique_ptr<VariableAttribute> attr);

    bool addMonitor(int id, bool transaction, float value, VariableMonitor::Type type, int severity);
};

}

#endif //MO_ENABLE_V201
#endif
