// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_GETBASEREPORT_H
#define MO_GETBASEREPORT_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <string>

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/Variables/Variable.h>

namespace MicroOcpp {

class VariableService;

namespace Ocpp201 {



class GetBaseReport : public Operation {
private:
    VariableService& variableService;

    GenericDeviceModelStatus status;

    const char *errorCode = nullptr;
public:
    GetBaseReport(VariableService& variableService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;

    const char *getErrorCode() override {return errorCode;}

};

} //namespace Ocpp201
} //namespace MicroOcpp

#endif //MO_ENABLE_V201

#endif
