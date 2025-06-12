// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_GETDIAGNOSTICS_H
#define MO_GETDIAGNOSTICS_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/Diagnostics/Diagnostics.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16 && MO_ENABLE_DIAGNOSTICS

namespace MicroOcpp {

class Context;
class DiagnosticsService;

namespace Ocpp16 {

class GetDiagnostics : public Operation, public MemoryManaged {
private:
    Context& context;
    DiagnosticsService& diagService;
    char filename [MO_GETLOG_FNAME_SIZE] = {'\0'};

    const char *errorCode = nullptr;
public:
    GetDiagnostics(Context& context, DiagnosticsService& diagService);

    const char* getOperationType() override {return "GetDiagnostics";}

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //namespace Ocpp16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16 && MO_ENABLE_DIAGNOSTICS
#endif
