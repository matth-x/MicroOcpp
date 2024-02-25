// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

/*
 * Implementation of the UCs B05 - B06
 */

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Operations/SetVariables.h>
#include <MicroOcpp/Operations/GetVariables.h>

#include <cstring>
#include <cctype>

#include <MicroOcpp/Debug.h>

namespace MicroOcpp {

template <class T>
VariableValidator<T>::VariableValidator(const ComponentId& component, const char *name, std::function<bool(T)> validate) :
        component(component), name(name), validate(validate) {

}

template <class T>
VariableValidator<T> *getVariableValidator(std::vector<VariableValidator<T>>& collection, const ComponentId& component, const char *name) {
    for (auto& validator : collection) {
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

std::unique_ptr<VariableContainer> VariableService::createContainer(const char *filename, bool accessible) const {
    //create non-persistent Variable store (i.e. lives only in RAM) if
    //     - Flash FS usage is switched off OR
    //     - Filename starts with "/volatile"
//    if (!filesystem ||
//                 !strncmp(filename, MO_VARIABLE_VOLATILE, strlen(MO_VARIABLE_VOLATILE))) {
        return makeVariableContainerVolatile(filename, accessible);
//    } else {
//        //create persistent Variable store. This is the normal case
//        return makeVariableContainerFlash(filesystem, filename, accessible);
//    }
}

void VariableService::addContainer(std::shared_ptr<VariableContainer> container) {
    containers.push_back(std::move(container));
}

std::shared_ptr<VariableContainer> VariableService::getContainer(const char *filename) {
    for (auto& container : containers) {
        if (!strcmp(filename, container->getFilename())) {
            return container;
        }
    }
    return nullptr;
}

template <class T>
bool registerVariableValidator(std::vector<VariableValidator<T>>& collection, const ComponentId& component, const char *name, std::function<bool(T)> validate) {
    for (auto it = collection.begin(); it != collection.end(); it++) {
        if (!strcmp(name, it->name) && component.equals(it->component)) {
            collection.erase(it);
            break;
        }
    }
    collection.emplace_back(component, name, validate);
    return true;
}

template <>
bool VariableService::registerValidator<int>(const ComponentId& component, const char *name, std::function<bool(int)> validate) {
    return registerVariableValidator<int>(validatorInt, component, name, validate);
}

template <>
bool VariableService::registerValidator<bool>(const ComponentId& component, const char *name, std::function<bool(bool)> validate) {
    return registerVariableValidator<bool>(validatorBool, component, name, validate);
}

template <>
bool VariableService::registerValidator<const char*>(const ComponentId& component, const char *name, std::function<bool(const char*)> validate) {
    return registerVariableValidator<const char*>(validatorString, component, name, validate);
}

VariableContainer *VariableService::declareContainer(const char *filename, bool accessible) {

    auto container = getContainer(filename);
    
    if (!container) {
        MO_DBG_DEBUG("init new configurations container: %s", filename);

        container = createContainer(filename, accessible);
        if (!container) {
            MO_DBG_ERR("OOM");
            return nullptr;
        }
        containers.push_back(container);
    }

    if (container->isAccessible() != accessible) {
        MO_DBG_ERR("%s: conflicting accessibility declarations (expect %s)", filename, container->isAccessible() ? "accessible" : "inaccessible");
        (void)0;
    }

    return container.get();
}

Variable *VariableService::getVariable(Variable::InternalDataType type, const ComponentId& component, const char *name, bool accessible) {
    for (auto& container : containers) {
        if (auto variable = container->getVariable(component, name)) {
            if (variable->isDetached()) {
                continue;
            }
            if (variable->getInternalDataType() != type) {
                MO_DBG_ERR("conflicting type for %s - remove old variable", name);
                variable->detach();
                continue;
            }
            if (container->isAccessible() != accessible) {
                MO_DBG_ERR("conflicting accessibility for %s", name);
                (void)0;
            }
            return variable;
        }
    }
    return nullptr;
}

VariableService::VariableService(Context& context, std::shared_ptr<FilesystemAdapter> filesystem) : filesystem(filesystem) {
    context.getOperationRegistry().registerOperation("SetVariables", [this] () {
        return new Ocpp201::SetVariables(*this);});
    context.getOperationRegistry().registerOperation("GetVariables", [this] () {
        return new Ocpp201::GetVariables(*this);});
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

void loadVariableCharacteristics(Variable& variable, Variable::Mutability mutability, bool rebootRequired) {
    if (variable.getMutability() == Variable::Mutability::ReadWrite) {
        variable.setMutability(mutability);
    }

    if (rebootRequired) {
        variable.setRebootRequired();
    }
}

template<class T>
Variable::InternalDataType getInternalDataType();

template<> Variable::InternalDataType getInternalDataType<int>() {return Variable::InternalDataType::Int;}
template<> Variable::InternalDataType getInternalDataType<bool>() {return Variable::InternalDataType::Bool;}
template<> Variable::InternalDataType getInternalDataType<const char*>() {return Variable::InternalDataType::String;}

template<class T>
Variable *VariableService::declareVariable(const ComponentId& component, const char *name, T factoryDefault, const char *containerPath, Variable::Mutability mutability, Variable::AttributeTypeSet attributes, bool rebootRequired, bool accessible) {

    auto res = getVariable(getInternalDataType<T>(), component, name, accessible);
    if (!res) {
        auto container = declareContainer(containerPath, accessible);
        if (!container) {
            return nullptr;
        }

        auto variable = container->createVariable(getInternalDataType<T>(), attributes);
        if (!variable) {
            return nullptr;
        }

        variable->setName(name);
        variable->setComponentId(component);

        if (!loadVariableFactoryDefault<T>(*variable, factoryDefault)) {
            return nullptr;
        }

        res = variable.get();

        if (!container->add(std::move(variable))) {
            return nullptr;
        }
    }

    loadVariableCharacteristics(*res, mutability, rebootRequired);
    return res;
}

template Variable *VariableService::declareVariable<int>(const ComponentId&, const char*, int, const char*, Variable::Mutability, Variable::AttributeTypeSet, bool, bool);
template Variable *VariableService::declareVariable<bool>(const ComponentId&, const char*, bool, const char*, Variable::Mutability, Variable::AttributeTypeSet, bool, bool);
template Variable *VariableService::declareVariable<const char*>(const ComponentId&, const char*, const char*, const char*, Variable::Mutability, Variable::AttributeTypeSet, bool, bool);

bool VariableService::commit() {
    bool success = true;

    for (auto& container : containers) {
        if (!container->save()) {
            success = false;
        }
    }

    return success;
}

SetVariableStatus VariableService::setVariable(Variable::AttributeType attrType, const char *value, const ComponentId& component, const char *variableName) {

    Variable *variable = nullptr;

    bool foundComponent = false;
    for (const auto& container : containers) {
        if (!container->isAccessible()) {
            // container intended for internal use only
            continue;
        }
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

    if (variable->getMutability() == Variable::Mutability::ReadOnly) {
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
    for (const auto& container : containers) {
        for (size_t i = 0; i < container->size(); i++) {
            auto variable = container->getVariable(i);

            if (variable->getComponentId().equals(component)) {
                foundComponent = true;

                if (!strcmp(variable->getName(), variableName)) {
                    // found variable. Search terminated in this block

                    if (variable->getMutability() == Variable::Mutability::WriteOnly) {
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

} // namespace MicroOcpp

#endif // MO_ENABLE_V201
