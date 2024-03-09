// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_SETVARIABLES_H
#define MO_SETVARIABLES_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <string>
#include <vector>

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/Variables/Variable.h>

namespace MicroOcpp {

class VariableService;

namespace Ocpp201 {

// SetVariableDataType (2.44) and
// SetVariableResultType (2.45)
struct SetVariableData {
    // SetVariableDataType
    Variable::AttributeType attributeType = Variable::AttributeType::Actual;
    const char *attributeValue; // will become invalid after processReq
    std::string componentName;
    int componentEvseId = -1;
    int componentEvseConnectorId = -1;
    std::string variableName;

    // SetVariableResultType
    SetVariableStatus attributeStatus;
};

class SetVariables : public Operation {
private:
    VariableService& variableService;
    std::vector<SetVariableData> queries;

    const char *errorCode = nullptr;
public:
    SetVariables(VariableService& variableService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;

    const char *getErrorCode() override {return errorCode;}

};

} //namespace Ocpp201
} //namespace MicroOcpp

#endif //MO_ENABLE_V201

#endif
