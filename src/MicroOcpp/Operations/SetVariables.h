// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_SETVARIABLES_H
#define MO_SETVARIABLES_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Model/Variables/Variable.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

namespace MicroOcpp {
namespace v201 {

class VariableService;

// SetVariableDataType (2.44) and
// SetVariableResultType (2.45)
struct SetVariableData {
    // SetVariableDataType
    Variable::AttributeType attributeType = Variable::AttributeType::Actual;
    const char *attributeValue; // will become invalid after processReq
    String componentName;
    int componentEvseId = -1;
    int componentEvseConnectorId = -1;
    String variableName;

    // SetVariableResultType
    SetVariableStatus attributeStatus;

    SetVariableData(const char *memory_tag = nullptr);
};

class SetVariables : public Operation, public MemoryManaged {
private:
    VariableService& variableService;
    Vector<SetVariableData> queries;

    const char *errorCode = nullptr;
public:
    SetVariables(VariableService& variableService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}

};

} //namespace v201
} //namespace MicroOcpp

#endif //MO_ENABLE_V201

#endif
