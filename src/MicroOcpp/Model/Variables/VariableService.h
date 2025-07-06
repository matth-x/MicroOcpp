// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

/*
 * Implementation of the UCs B05 - B06
 */

#ifndef MO_VARIABLESERVICE_H
#define MO_VARIABLESERVICE_H

#include <stdint.h>
#include <memory>
#include <limits>

#include <MicroOcpp/Model/Variables/Variable.h>
#include <MicroOcpp/Model/Variables/VariableContainer.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#ifndef MO_VARIABLESTORE_FN_PREFIX
#define MO_VARIABLESTORE_FN_PREFIX "ocpp-vars-"
#endif

#ifndef MO_VARIABLESTORE_FN_SUFFIX
#define MO_VARIABLESTORE_FN_SUFFIX ".jsn"
#endif

#ifndef MO_VARIABLESTORE_BUCKETS
#define MO_VARIABLESTORE_BUCKETS 8
#endif

namespace MicroOcpp {

class Context;

namespace v201 {

template <class T>
struct VariableValidator {
    ComponentId component;
    const char *name;
    void *userPtr;
    bool (*validateFn)(T, void*);
    VariableValidator(const ComponentId& component, const char *name, bool (*validate)(T, void*), void *userPtr);
    bool validate(T);
};

class VariableService : public MemoryManaged {
private:
    Context& context;
    MO_FilesystemAdapter *filesystem = nullptr;
    Vector<VariableContainer*> containers;
    VariableContainerNonOwning containerExternal;
    VariableContainerOwning containersInternal [MO_VARIABLESTORE_BUCKETS];
    VariableContainerOwning& getContainerInternalByVariable(const ComponentId& component, const char *name);

    Vector<VariableValidator<int>> validatorInt;
    Vector<VariableValidator<bool>> validatorBool;
    Vector<VariableValidator<const char*>> validatorString;

    VariableValidator<int> *getValidatorInt(const ComponentId& component, const char *name);
    VariableValidator<bool> *getValidatorBool(const ComponentId& component, const char *name);
    VariableValidator<const char*> *getValidatorString(const ComponentId& component, const char *name);

    Vector<Variable*> getBaseReportVars;
    int getBaseReportRequestId = -1;
    unsigned int getBaseReportSeqNo = 0;
    bool notifyReportInProgress = false;
public:
    VariableService(Context& context);

    bool init();

    //Get Variable. If not existent, create Variable owned by MO and return
    template <class T> 
    Variable *declareVariable(const ComponentId& component, const char *name, T factoryDefault, Mutability mutability = Mutability::ReadWrite, bool persistent = true, Variable::AttributeTypeSet attributes = Variable::AttributeTypeSet(), bool rebootRequired = false);

    bool addVariable(Variable *variable); //Add Variable without transferring ownership
    bool addVariable(std::unique_ptr<Variable> variable); //Add Variable and transfer ownership

    //Get Variable. If not existent, return nullptr
    Variable *getVariable(const ComponentId& component, const char *name);

    void addContainer(VariableContainer *container);

    template <class T>
    bool registerValidator(const ComponentId& component, const char *name, bool (*validate)(T, void*), void *userPtr = nullptr);

    bool setup();

    void loop();

    bool commit();

    SetVariableStatus setVariable(Variable::AttributeType attrType, const char *attrVal, const ComponentId& component, const char *variableName);

    GetVariableStatus getVariable(Variable::AttributeType attrType, const ComponentId& component, const char *variableName, Variable **result);

    GenericDeviceModelStatus getBaseReport(int requestId, ReportBase reportBase);
};

} //namespace MicroOcpp
} //namespace v201
#endif //MO_ENABLE_V201
#endif
