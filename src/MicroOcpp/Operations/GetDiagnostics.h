// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef GETDIAGNOSTICS_H
#define GETDIAGNOSTICS_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Time.h>

namespace MicroOcpp {

class DiagnosticsService;

namespace Ocpp16 {

class GetDiagnostics : public Operation {
private:
    DiagnosticsService& diagService;
    std::string fileName;

    const char *errorCode = nullptr;
public:
    GetDiagnostics(DiagnosticsService& diagService);

    const char* getOperationType() override {return "GetDiagnostics";}

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace MicroOcpp

#endif
