// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

/*
 * Implementation of the UCs B05 - B06
 */

#ifndef MO_VARIABLESERVICE_H
#define MO_VARIABLESERVICE_H

#include <stdint.h>
#include <vector>
#include <memory>
#include <limits>
#include <functional>

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Model/Variables/Variable.h>
#include <MicroOcpp/Model/Variables/VariableContainer.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>

#ifndef MO_VARIABLE_FN
#define MO_VARIABLE_FN (MO_FILENAME_PREFIX "ocpp-vars.jsn")
#endif

#ifndef MO_VARIABLE_VOLATILE
#define MO_VARIABLE_VOLATILE "/volatile"
#endif

#ifndef MO_VARIABLE_INTERNAL_FN
#define MO_VARIABLE_INTERNAL_FN (MO_FILENAME_PREFIX "mo-vars.jsn")
#endif

namespace MicroOcpp {

template <class T>
struct VariableValidator {
    ComponentId component;
    const char *name;
    std::function<bool(T)> validate;
    VariableValidator(const ComponentId& component, const char *name, std::function<bool(T)> validate);
};

class Context;

class VariableService {
private:
    std::shared_ptr<FilesystemAdapter> filesystem;
    std::vector<std::shared_ptr<VariableContainer>> containers;

    std::vector<VariableValidator<int>> validatorInt;
    std::vector<VariableValidator<bool>> validatorBool;
    std::vector<VariableValidator<const char*>> validatorString;

    VariableValidator<int> *getValidatorInt(const ComponentId& component, const char *name);
    VariableValidator<bool> *getValidatorBool(const ComponentId& component, const char *name);
    VariableValidator<const char*> *getValidatorString(const ComponentId& component, const char *name);

    std::unique_ptr<VariableContainer> createContainer(const char *filename, bool accessible) const;

    VariableContainer *declareContainer(const char *filename, bool accessible);

    Variable *getVariable(Variable::InternalDataType type, const ComponentId& component, const char *name, bool accessible);

public:
    VariableService(Context& context, std::shared_ptr<FilesystemAdapter> filesystem);

    template <class T> 
    Variable *declareVariable(const ComponentId& component, const char *name, T factoryDefault, const char *containerPath = MO_VARIABLE_FN, Variable::Mutability mutability = Variable::Mutability::ReadWrite, Variable::AttributeTypeSet attributes = Variable::AttributeTypeSet(), bool rebootRequired = false, bool accessible = true);

    bool commit();

    void addContainer(std::shared_ptr<VariableContainer> container);

    std::shared_ptr<VariableContainer> getContainer(const char *filename);

    template <class T>
    bool registerValidator(const ComponentId& component, const char *name, std::function<bool(T)> validate);

    SetVariableStatus setVariable(Variable::AttributeType attrType, const char *attrVal, const ComponentId& component, const char *variableName);

    GetVariableStatus getVariable(Variable::AttributeType attrType, const ComponentId& component, const char *variableName, Variable **result);
};

} // namespace MicroOcpp

#endif // MO_ENABLE_V201

#endif
