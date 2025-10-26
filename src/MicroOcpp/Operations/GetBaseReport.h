// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_GETBASEREPORT_H
#define MO_GETBASEREPORT_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Model/Variables/Variable.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

namespace MicroOcpp {
namespace v201 {

class VariableService;

class GetBaseReport : public Operation, public MemoryManaged {
private:
    VariableService& variableService;

    GenericDeviceModelStatus status;

    const char *errorCode = nullptr;
public:
    GetBaseReport(VariableService& variableService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}

};

} //namespace v201
} //namespace MicroOcpp
#endif //MO_ENABLE_V201
#endif
