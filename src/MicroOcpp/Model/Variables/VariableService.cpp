// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

/*
 * Implementation of the UCs B05 - B06
 */

#include <string.h>
#include <ctype.h>

#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Operations/SetVariables.h>
#include <MicroOcpp/Operations/GetVariables.h>
#include <MicroOcpp/Operations/GetBaseReport.h>
#include <MicroOcpp/Operations/NotifyReport.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V201

#ifndef MO_GETBASEREPORT_CHUNKSIZE
#define MO_GETBASEREPORT_CHUNKSIZE 4
#endif

namespace MicroOcpp {
namespace v201 {

template <class T>
VariableValidator<T>::VariableValidator(const ComponentId& component, const char *name, bool (*validateFn)(T, void*), void *userPtr) :
        component(component), name(name), userPtr(userPtr), validateFn(validateFn) {

}

template <class T>
bool VariableValidator<T>::validate(T v) {
    return validateFn(v, userPtr);
}

template <class T>
VariableValidator<T> *getVariableValidator(Vector<VariableValidator<T>>& collection, const ComponentId& component, const char *name) {
    for (size_t i = 0; i < collection.size(); i++) {
        auto& validator = collection[i];
        if (!strcmp(name, validator.name) && component.equals(validator.component)) {
            return &validator;
        }
    }
    return nullptr;
}

VariableValidator<int> *VariableService::getValidatorInt(const ComponentId& component, const char *name) {
    return getVariableValidator<int>(validatorInt, component, name);
}

VariableValidator<bool> *VariableService::getValidatorBool(const ComponentId& component, const char *name) {
    return getVariableValidator<bool>(validatorBool, component, name);
}

VariableValidator<const char*> *VariableService::getValidatorString(const ComponentId& component, const char *name) {
    return getVariableValidator<const char*>(validatorString, component, name);
}

VariableContainerOwning& VariableService::getContainerInternalByVariable(const ComponentId& component, const char *name) {
    unsigned int hash = 0;
    for (size_t i = 0; i < strlen(component.name); i++) {
        hash += (unsigned int)component.name[i];
    }
    if (component.evse.id >= 0)
        hash += (unsigned int)component.evse.id;
    if (component.evse.connectorId >= 0)
        hash += (unsigned int)component.evse.connectorId;
    for (size_t i = 0; i < strlen(name); i++) {
        hash += (unsigned int)name[i];
    }
    return containersInternal[hash % MO_VARIABLESTORE_BUCKETS];
}

void VariableService::addContainer(VariableContainer *container) {
    containers.push_back(container);
}

template <class T>
bool registerVariableValidator(Vector<VariableValidator<T>>& collection, const ComponentId& component, const char *name, bool (*validate)(T, void*), void *userPtr) {
    for (auto it = collection.begin(); it != collection.end(); it++) {
        if (!strcmp(name, it->name) && component.equals(it->component)) {
            collection.erase(it);
            break;
        }
    }
    collection.emplace_back(component, name, validate, userPtr);
    return true;
}

template <>
bool VariableService::registerValidator<int>(const ComponentId& component, const char *name, bool (*validate)(int, void*), void *userPtr) {
    return registerVariableValidator<int>(validatorInt, component, name, validate, userPtr);
}

template <>
bool VariableService::registerValidator<bool>(const ComponentId& component, const char *name, bool (*validate)(bool, void*), void *userPtr) {
    return registerVariableValidator<bool>(validatorBool, component, name, validate, userPtr);
}

template <>
bool VariableService::registerValidator<const char*>(const ComponentId& component, const char *name, bool (*validate)(const char*, void*), void *userPtr) {
    return registerVariableValidator<const char*>(validatorString, component, name, validate, userPtr);
}

Variable *VariableService::getVariable(const ComponentId& component, const char *name) {

    if (auto variable = getContainerInternalByVariable(component, name).getVariable(component, name)) {
        return variable;
    }

    for (size_t i = 0; i < containers.size(); i++) {
        auto container = containers[containers.size() - i - 1]; //search from back, because internal containers at front don't contain variable
        if (auto variable = container->getVariable(component, name)) {
            return variable;
        }
    }

    return nullptr;
}

VariableService::VariableService(Context& context) :
            MemoryManaged("v201.Variables.VariableService"),
            context(context),
            containers(makeVector<VariableContainer*>(getMemoryTag())),
            validatorInt(makeVector<VariableValidator<int>>(getMemoryTag())),
            validatorBool(makeVector<VariableValidator<bool>>(getMemoryTag())),
            validatorString(makeVector<VariableValidator<const char*>>(getMemoryTag())),
            getBaseReportVars(makeVector<Variable*>(getMemoryTag())) {

}

bool VariableService::init() {
    containers.reserve(MO_VARIABLESTORE_BUCKETS + 1);
    if (containers.capacity() < MO_VARIABLESTORE_BUCKETS + 1) {
        MO_DBG_ERR("OOM");
        return false;
    }

    for (unsigned int i = 0; i < MO_VARIABLESTORE_BUCKETS; i++) {
        containers.push_back(&containersInternal[i]);
    }
    containers.push_back(&containerExternal);

    return true;
}

bool VariableService::setup() {

    filesystem = context.getFilesystem();
    if (!filesystem) {
        MO_DBG_DEBUG("no fs access");
    }

    for (unsigned int i = 0; i < MO_VARIABLESTORE_BUCKETS; i++) {
        char fn [MO_MAX_PATH_SIZE];
        auto ret = snprintf(fn, sizeof(fn), "%s%02x%s", MO_VARIABLESTORE_FN_PREFIX, i, MO_VARIABLESTORE_FN_SUFFIX);
        if (ret < 0 || (size_t)ret >= sizeof(fn)) {
            MO_DBG_ERR("fn error");
            return false;
        }
        containersInternal[i].enablePersistency(filesystem, fn);

        if (!containersInternal[i].load()) {
            MO_DBG_ERR("failure to load %");
            return false;
        }
    }

    context.getMessageService().registerOperation("SetVariables", [] (Context& context) -> Operation* {
        return new SetVariables(*context.getModel201().getVariableService());});
    context.getMessageService().registerOperation("GetVariables", [] (Context& context) -> Operation* {
        return new GetVariables(*context.getModel201().getVariableService());});
    context.getMessageService().registerOperation("GetBaseReport", [] (Context& context) -> Operation* {
        return new GetBaseReport(*context.getModel201().getVariableService());});

    return true;
}

void VariableService::loop() {

    if (notifyReportInProgress) {
        return;
    }

    if (getBaseReportVars.empty()) {
        // all done
        return;
    }

    auto variablesChunk = makeVector<Variable*>(getMemoryTag());
    variablesChunk.reserve(MO_GETBASEREPORT_CHUNKSIZE);
    if (variablesChunk.capacity() < MO_GETBASEREPORT_CHUNKSIZE) {
        MO_DBG_ERR("OOM");
        getBaseReportVars.clear();
        return;
    }

    for (size_t i = 0; i < MO_GETBASEREPORT_CHUNKSIZE && !getBaseReportVars.empty(); i++) {
        variablesChunk.push_back(getBaseReportVars.back());
        getBaseReportVars.pop_back();
    }

    auto notifyReport = makeRequest(context, new NotifyReport(
            context,
            getBaseReportRequestId,
            context.getClock().now(),
            !getBaseReportVars.empty(), // tbc: to be continued
            getBaseReportSeqNo,
            variablesChunk));

    if (!notifyReport) {
        MO_DBG_ERR("OOM");
        getBaseReportVars.clear();
        return;
    }

    getBaseReportSeqNo++;

    notifyReport->setOnReceiveConf([this] (JsonObject) {
        notifyReportInProgress = false;
    });
    notifyReport->setOnAbort([this] () {
        notifyReportInProgress = false;
    });

    notifyReportInProgress = true;

    context.getMessageService().sendRequestPreBoot(std::move(notifyReport));
}

template<class T>
bool loadVariableFactoryDefault(Variable& variable, T factoryDef);

template<>
bool loadVariableFactoryDefault<int>(Variable& variable, int factoryDef) {
    variable.setInt(factoryDef);
    return true;
}

template<>
bool loadVariableFactoryDefault<bool>(Variable& variable, bool factoryDef) {
    variable.setBool(factoryDef);
    return true;
}

template<>
bool loadVariableFactoryDefault<const char*>(Variable& variable, const char *factoryDef) {
    return variable.setString(factoryDef);
}

void loadVariableCharacteristics(Variable& variable, Mutability mutability, bool persistent, bool rebootRequired, Variable::InternalDataType defaultDataType) {
    if (variable.getMutability() == Mutability::ReadWrite) {
        variable.setMutability(mutability);
    }

    if (persistent) {
        variable.setPersistent();
    }

    if (rebootRequired) {
        variable.setRebootRequired();
    }

    switch (defaultDataType) {
        case Variable::InternalDataType::Int:
            variable.setVariableDataType(VariableCharacteristics::DataType::integer);
            break;
        case Variable::InternalDataType::Bool:
            variable.setVariableDataType(VariableCharacteristics::DataType::boolean);
            break;
        case Variable::InternalDataType::String:
            variable.setVariableDataType(VariableCharacteristics::DataType::string);
            break;
        default:
            MO_DBG_ERR("internal error");
            break;
    }
}

template<class T>
Variable::InternalDataType getInternalDataType();

template<> Variable::InternalDataType getInternalDataType<int>() {return Variable::InternalDataType::Int;}
template<> Variable::InternalDataType getInternalDataType<bool>() {return Variable::InternalDataType::Bool;}
template<> Variable::InternalDataType getInternalDataType<const char*>() {return Variable::InternalDataType::String;}

template<class T>
Variable *VariableService::declareVariable(const ComponentId& component, const char *name, T factoryDefault, Mutability mutability, bool persistent, Variable::AttributeTypeSet attributes, bool rebootRequired) {

    auto res = getVariable(component, name);
    if (!res) {
        auto variable = makeVariable(getInternalDataType<T>(), attributes);
        if (!variable) {
            MO_DBG_ERR("OOM");
            return nullptr;
        }

        variable->setName(name);
        variable->setComponentId(component);

        if (!loadVariableFactoryDefault<T>(*variable, factoryDefault)) {
            return nullptr;
        }

        res = variable.get();

        if (!getContainerInternalByVariable(component, name).add(std::move(variable))) {
            return nullptr;
        }
    }

    loadVariableCharacteristics(*res, mutability, persistent, rebootRequired, getInternalDataType<T>());
    return res;
}

template Variable *VariableService::declareVariable<int>(        const ComponentId&, const char*, int,         Mutability, bool, Variable::AttributeTypeSet, bool);
template Variable *VariableService::declareVariable<bool>(       const ComponentId&, const char*, bool,        Mutability, bool, Variable::AttributeTypeSet, bool);
template Variable *VariableService::declareVariable<const char*>(const ComponentId&, const char*, const char*, Mutability, bool, Variable::AttributeTypeSet, bool);

bool VariableService::addVariable(Variable *variable) {
    return containerExternal.add(variable);
}

bool VariableService::addVariable(std::unique_ptr<Variable> variable) {
    return getContainerInternalByVariable(variable->getComponentId(), variable->getName()).add(std::move(variable));
}

bool VariableService::commit() {
    bool success = true;

    for (size_t i = 0; i < containers.size(); i++) {
        if (!containers[i]->commit()) {
            success = false;
        }
    }

    return success;
}

SetVariableStatus VariableService::setVariable(Variable::AttributeType attrType, const char *value, const ComponentId& component, const char *variableName) {

    Variable *variable = nullptr;

    bool foundComponent = false;
    for (size_t i = 0; i < containers.size(); i++) {
        auto container = containers[i];

        for (size_t i = 0; i < container->size(); i++) {
            auto entry = container->getVariable(i);

            if (entry->getComponentId().equals(component)) {
                foundComponent = true;

                if (!strcmp(entry->getName(), variableName)) {
                    // found variable. Search terminated in this block

                    variable = entry;
                    break;
                }
            }
        }
        if (variable) {
            // result found in inner for-loop
            break;
        }
    }

    if (!variable) {
        if (foundComponent) {
            return SetVariableStatus::UnknownVariable;
        } else {
            return SetVariableStatus::UnknownComponent;
        }
    }

    if (variable->getMutability() == Mutability::ReadOnly) {
        return SetVariableStatus::Rejected;
    }

    if (!variable->hasAttribute(attrType)) {
        return SetVariableStatus::NotSupportedAttributeType;
    }

    //write config

    /*
     * Try to interpret input as number
     */

    bool convertibleInt = true;
    int numInt = 0;
    bool convertibleBool = true;
    bool numBool = false;

    int nDigits = 0, nNonDigits = 0, nDots = 0, nSign = 0; //"-1.234" has 4 digits, 0 nonDigits, 1 dot and 1 sign. Don't allow comma as seperator. Don't allow e-expressions (e.g. 1.23e-7)
    for (const char *c = value; *c; ++c) {
        if (*c >= '0' && *c <= '9') {
            //int interpretation
            if (nDots == 0) { //only append number if before floating point
                nDigits++;
                numInt *= 10;
                numInt += *c - '0';
            }
        } else if (*c == '.') {
            nDots++;
        } else if (c == value && *c == '-') {
            nSign++;
        } else {
            nNonDigits++;
        }
    }

    if (nSign == 1) {
        numInt = -numInt;
    }

    int INT_MAXDIGITS; //plausibility check: this allows a numerical range of (-999,999,999 to 999,999,999), or (-9,999 to 9,999) respectively
    if (sizeof(int) >= 4UL)
        INT_MAXDIGITS = 9;
    else
        INT_MAXDIGITS = 4;

    if (nNonDigits > 0 || nDigits == 0 || nSign > 1 || nDots > 1) {
        convertibleInt = false;
    }

    if (nDigits > INT_MAXDIGITS) {
        MO_DBG_DEBUG("Possible integer overflow: key = %s, value = %s", variableName, value);
        convertibleInt = false;
    }

    if (tolower(value[0]) == 't' && tolower(value[1]) == 'r' && tolower(value[2]) == 'u' && tolower(value[3]) == 'e' && !value[4]) {
        numBool = true;
    } else if (tolower(value[0]) == 'f' && tolower(value[1]) == 'a' && tolower(value[2]) == 'l' && tolower(value[3]) == 's' && tolower(value[4]) == 'e' && !value[5]) {
        numBool = false;
    } else if (convertibleInt) {
        numBool = numInt != 0;
    } else {
        convertibleBool = false;
    }

    // validate and store (parsed) value to Config

    if (variable->getInternalDataType() == Variable::InternalDataType::Int && convertibleInt) {
        auto validator = getValidatorInt(component, variableName);
        if (validator && !validator->validate(numInt)) {
            MO_DBG_WARN("validation failed for variable=%s", variableName);
            return SetVariableStatus::Rejected;
        }
        variable->setInt(numInt);
    } else if (variable->getInternalDataType() == Variable::InternalDataType::Bool && convertibleBool) {
        auto validator = getValidatorBool(component, variableName);
        if (validator && !validator->validate(numBool)) {
            MO_DBG_WARN("validation failed for variable=%s", variableName);
            return SetVariableStatus::Rejected;
        }
        variable->setBool(numBool);
    } else if (variable->getInternalDataType() == Variable::InternalDataType::String) {
        auto validator = getValidatorString(component, variableName);
        if (validator && !validator->validate(value)) {
            MO_DBG_WARN("validation failed for variable=%s", variableName);
            return SetVariableStatus::Rejected;
        }
        variable->setString(value);
    } else {
        MO_DBG_WARN("Value has incompatible type");
        return SetVariableStatus::Rejected;
    }

    if (variable->isRebootRequired()) {
        return SetVariableStatus::RebootRequired;
    }

    return SetVariableStatus::Accepted;
}

GetVariableStatus VariableService::getVariable(Variable::AttributeType attrType, const ComponentId& component, const char *variableName, Variable **result) {

    bool foundComponent = false;
    for (size_t i = 0; i < containers.size(); i++) {
        auto container = containers[i];

        for (size_t i = 0; i < container->size(); i++) {
            auto variable = container->getVariable(i);

            if (variable->getComponentId().equals(component)) {
                foundComponent = true;

                if (!strcmp(variable->getName(), variableName)) {
                    // found variable. Search terminated in this block

                    if (variable->getMutability() == Mutability::WriteOnly) {
                        return GetVariableStatus::Rejected;
                    }

                    if (variable->hasAttribute(attrType)) {
                        *result = variable;
                        return GetVariableStatus::Accepted;
                    } else {
                        return GetVariableStatus::NotSupportedAttributeType;
                    }
                }
            }
        }
    }

    if (foundComponent) {
        return GetVariableStatus::UnknownVariable;
    } else {
        return GetVariableStatus::UnknownComponent;
    }
}

GenericDeviceModelStatus VariableService::getBaseReport(int requestId, ReportBase reportBase) {

    if (reportBase == ReportBase_SummaryInventory) {
        return GenericDeviceModelStatus_NotSupported;
    }

    if (!getBaseReportVars.empty()) {
        MO_DBG_ERR("request new base report while old is still pending");
        return GenericDeviceModelStatus_Rejected;
    }

    for (size_t i = 0; i < containers.size(); i++) {
        auto container = containers[i];

        for (size_t i = 0; i < container->size(); i++) {
            auto variable = container->getVariable(i);

            if (reportBase == ReportBase_ConfigurationInventory && variable->getMutability() == Mutability::ReadOnly) {
                continue;
            }

            getBaseReportVars.push_back(variable);
        }
    }

    if (getBaseReportVars.empty()) {
        return GenericDeviceModelStatus_EmptyResultSet;
    }

    getBaseReportRequestId = requestId;
    getBaseReportSeqNo = 0;

    return GenericDeviceModelStatus_Accepted;
}

} //namespace v201
} //namespace MicroOcpp
#endif //MO_ENABLE_V201
