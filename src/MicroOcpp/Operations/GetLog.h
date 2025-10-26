// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_GETLOG_H
#define MO_GETLOG_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/Diagnostics/Diagnostics.h>
#include <MicroOcpp/Version.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_DIAGNOSTICS

namespace MicroOcpp {

class Context;
class DiagnosticsService;

class GetLog : public Operation, public MemoryManaged {
private:
    Context& context;
    DiagnosticsService& diagService;
    MO_GetLogStatus status = MO_GetLogStatus_UNDEFINED;
    char filename [MO_GETLOG_FNAME_SIZE] = {'\0'};

    const char *errorCode = nullptr;
public:
    GetLog(Context& context, DiagnosticsService& diagService);

    const char* getOperationType() override {return "GetLog";}

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //namespace MicroOcpp
#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_DIAGNOSTICS
#endif
