// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_GETVARIABLES_H
#define MO_GETVARIABLES_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Model/Variables/Variable.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

namespace MicroOcpp {    
namespace Ocpp201 {

class VariableService;

// GetVariableDataType (2.25) and
// GetVariableResultType (2.26)
struct GetVariableData {
    // GetVariableDataType
    Variable::AttributeType attributeType = Variable::AttributeType::Actual;
    String componentName;
    int componentEvseId = -1;
    int componentEvseConnectorId = -1;
    String variableName;

    // GetVariableResultType
    GetVariableStatus attributeStatus;
    Variable *variable = nullptr;

    GetVariableData(const char *memory_tag = nullptr);
};

class GetVariables : public Operation, public MemoryManaged {
private:
    VariableService& variableService;
    Vector<GetVariableData> queries;

    const char *errorCode = nullptr;
public:
    GetVariables(VariableService& variableService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //namespace Ocpp201
} //namespace MicroOcpp
#endif //MO_ENABLE_V201

#endif
