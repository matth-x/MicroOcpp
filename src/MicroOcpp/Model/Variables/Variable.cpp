// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

/*
 * Implementation of the UCs B05 - B06
 */

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Model/Variables/Variable.h>

#include <string.h>

#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

ComponentId::ComponentId(const char *name) : name(name) { }
ComponentId::ComponentId(const char *name, EvseId evse) : name(name), evse(evse) { }

bool ComponentId::equals(const ComponentId& other) const {
    return !strcmp(name, other.name) &&
        ((evse.id < 0 && other.evse.id < 0) || (evse.id == other.evse.id)) && // evseId undefined or equal
        ((evse.connectorId < 0 && other.evse.connectorId < 0) || (evse.connectorId == other.evse.connectorId)); // connectorId undefined or equal
}

bool Variable::AttributeTypeSet::has(AttributeType type) {
    switch(type) {
        case AttributeType::Actual:
            return flag & (1 << 0);
        case AttributeType::Target:
            return flag & (1 << 1);
        case AttributeType::MinSet:
            return flag & (1 << 2);
        case AttributeType::MaxSet:
            return flag & (1 << 3);
    }
    MO_DBG_ERR("internal error");
    return false;
}

Variable::AttributeTypeSet& Variable::AttributeTypeSet::set(AttributeType type) {
    switch(type) {
        case AttributeType::Actual:
            flag |= (1 << 0);
            break;
        case AttributeType::Target:
            flag |= (1 << 1);
            break;
        case AttributeType::MinSet:
            flag |= (1 << 2);
            break;
        case AttributeType::MaxSet:
            flag |= (1 << 3);
            break;
        default:
            MO_DBG_ERR("internal error");
            break;
    }
    return *this;
}

size_t Variable::AttributeTypeSet::count() {
    return (flag & (1 << 0) ? 1 : 0) +
           (flag & (1 << 1) ? 1 : 0) +
           (flag & (1 << 2) ? 1 : 0) +
           (flag & (1 << 3) ? 1 : 0);
}

Variable::AttributeTypeSet::AttributeTypeSet(AttributeType attrType) {
    set(attrType);
}

Variable::Variable(AttributeTypeSet attributes) : attributes(attributes) { }

Variable::~Variable() {

}

void Variable::setName(const char *name) {
    this->variableName = name;
}
const char *Variable::getName() const {
    return variableName;
}

void Variable::setComponentId(const ComponentId& componentId) {
    this->component = componentId;
}
const ComponentId& Variable::getComponentId() const {
    return component;
}

void Variable::setInt(int val, AttributeType) {
    MO_DBG_ERR("type err");
    (void)0;
}
void Variable::setBool(bool val, AttributeType) {
    MO_DBG_ERR("type err");
    (void)0;
}
bool Variable::setString(const char *val, AttributeType) {
    MO_DBG_ERR("type err");
    return false;
}

int Variable::getInt(AttributeType) {
    MO_DBG_ERR("type err");
    return 0;
}
bool Variable::getBool(AttributeType) {
    MO_DBG_ERR("type err");
    return false;
}
const char *Variable::getString(AttributeType) {
    MO_DBG_ERR("type err");
    return nullptr;
}

bool Variable::hasAttribute(AttributeType attrType) {
    return attributes.has(attrType);
}

void Variable::setVariableDataType(VariableCharacteristics::DataType dataType) {
    this->dataType = dataType;
}
VariableCharacteristics::DataType Variable::getVariableDataType() {
    return dataType;
}

bool Variable::getSupportsMonitoring() {
    return supportsMonitoring;
}
void Variable::setSupportsMonitoring() {
    supportsMonitoring = true;
}

bool Variable::isRebootRequired() {
    return rebootRequired;
}
void Variable::setRebootRequired() {
    rebootRequired = true;
}

void Variable::setMutability(Mutability m) {
    this->mutability = m;
}
Variable::Mutability Variable::getMutability() {
    return mutability;
}

void Variable::setPersistent() {
    persistent = true;
}
bool Variable::isPersistent() {
    return persistent;
}

void Variable::setConstant() {
    constant = true;
}
bool Variable::isConstant() {
    return constant;
}

template <class T>
struct VariableSingleData {
    T value = 0;

    T& get(Variable::AttributeType attribute) {
        return value;
    }
};

template <class T>
struct VariableFullData {
    T actual = 0;
    T target = 0;
    T minSet = 0;
    T maxSet = 0;

    T& get(Variable::AttributeType attribute) {
        switch(attribute) {
            case Variable::AttributeType::Actual:
                return actual;
            case Variable::AttributeType::Target:
                return target;
            case Variable::AttributeType::MinSet:
                return minSet;
            case Variable::AttributeType::MaxSet:
                return maxSet;
        }
        MO_DBG_ERR("internal error");
        return actual;
    }
};

template <template<class T> class VariableData>
class VariableInt : public Variable {
private:
    VariableData<int> value;
    uint16_t writeCount = 0;

    #if MO_VARIABLE_TYPECHECK
    AttributeTypeSet attributes;
    #endif
public:
    VariableInt(AttributeTypeSet attributes) :
        Variable(attributes)
        #if MO_VARIABLE_TYPECHECK
        , attributes(attributes)
        #endif
    {

    }

    void setInt(int val, AttributeType attrType) override {
        #if MO_VARIABLE_TYPECHECK
        if (!attributes.has(attrType)) {
            MO_DBG_ERR("type err");
            return;
        }
        #endif
        value.get(attrType) = val;
        writeCount++;
    }

    int getInt(AttributeType attrType) override {
        #if MO_VARIABLE_TYPECHECK
        if (!attributes.has(attrType)) {
            MO_DBG_ERR("type err");
            return 0;
        }
        #endif
        return value.get(attrType);
    }

    InternalDataType getInternalDataType() override {
        return InternalDataType::Int;
    }

    uint16_t getWriteCount() override {
        return writeCount;
    }
};

template <template<class T> class VariableData>
class VariableBool : public Variable {
private:
    VariableData<bool> value;
    uint16_t writeCount = 0;

    #if MO_VARIABLE_TYPECHECK
    AttributeTypeSet attributes;
    #endif
public:
    VariableBool(AttributeTypeSet attributes) :
        Variable(attributes)
        #if MO_VARIABLE_TYPECHECK
        , attributes(attributes)
        #endif
    {

    }

    void setBool(bool val, AttributeType attrType) override {
        #if MO_VARIABLE_TYPECHECK
        if (!attributes.has(attrType)) {
            MO_DBG_ERR("type err");
            return;
        }
        #endif
        value.get(attrType) = val;
        writeCount++;
    }

    bool getBool(AttributeType attrType) override {
        #if MO_VARIABLE_TYPECHECK
        if (!attributes.has(attrType)) {
            MO_DBG_ERR("type err");
            return 0;
        }
        #endif
        return value.get(attrType);
    }

    InternalDataType getInternalDataType() override {
        return InternalDataType::Bool;
    }

    uint16_t getWriteCount() override {
        return writeCount;
    }
};

template <template<class T> class VariableData>
class VariableString : public Variable {
private:
    VariableData<char*> value;
    uint16_t writeCount = 0;

    #if MO_VARIABLE_TYPECHECK
    AttributeTypeSet attributes;
    #endif
public:
    VariableString(AttributeTypeSet attributes) :
        Variable(attributes)
        #if MO_VARIABLE_TYPECHECK
        , attributes(attributes)
        #endif
    {

    }

    ~VariableString() {
        delete[] value.get(AttributeType::Actual);
        value.get(AttributeType::Actual) = nullptr;
        delete[] value.get(AttributeType::Target);
        value.get(AttributeType::Target) = nullptr;
        delete[] value.get(AttributeType::MinSet);
        value.get(AttributeType::MinSet) = nullptr;
        delete[] value.get(AttributeType::MaxSet);
        value.get(AttributeType::MaxSet) = nullptr;
    }

    bool setString(const char *val, AttributeType attrType) override {
        #if MO_VARIABLE_TYPECHECK
        if (!attributes.has(attrType)) {
            MO_DBG_ERR("type err");
            return false;
        }
        #endif

        size_t len = strlen(val);
        char *valNew = nullptr;
        if (len != 0) {
            valNew = new char[len + 1];
            if (!valNew) {
                MO_DBG_ERR("OOM");
                return false;
            }
        }
        delete[] value.get(attrType);
        value.get(attrType) = valNew;
        writeCount++;
        return true;
    }

    const char *getString(AttributeType attrType) override {
        #if MO_VARIABLE_TYPECHECK
        if (!attributes.has(attrType)) {
            MO_DBG_ERR("type err");
            return 0;
        }
        #endif
        return value.get(attrType) ? value.get(attrType) : "";
    }

    InternalDataType getInternalDataType() override {
        return InternalDataType::String;
    }

    uint16_t getWriteCount() override {
        return writeCount;
    }
};

std::unique_ptr<Variable> MicroOcpp::makeVariable(Variable::InternalDataType dtype, Variable::AttributeTypeSet supportAttributes) {
    switch(dtype) {
        case Variable::InternalDataType::Int:
            if (supportAttributes.count() > 1) {
                return std::unique_ptr<Variable>(new VariableInt<VariableFullData>(supportAttributes));
            } else {
                return std::unique_ptr<Variable>(new VariableInt<VariableSingleData>(supportAttributes));
            }
        case Variable::InternalDataType::Bool:
            if (supportAttributes.count() > 1) {
                return std::unique_ptr<Variable>(new VariableBool<VariableFullData>(supportAttributes));
            } else {
                return std::unique_ptr<Variable>(new VariableBool<VariableSingleData>(supportAttributes));
            }
        case Variable::InternalDataType::String:
            if (supportAttributes.count() > 1) {
                return std::unique_ptr<Variable>(new VariableString<VariableFullData>(supportAttributes));
            } else {
                return std::unique_ptr<Variable>(new VariableString<VariableSingleData>(supportAttributes));
            }
    }

    MO_DBG_ERR("internal error");
    return nullptr;
}

#endif // MO_ENABLE_V201
