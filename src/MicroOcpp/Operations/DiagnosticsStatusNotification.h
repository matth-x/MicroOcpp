// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_DIAGNOSTICSSTATUSNOTIFICATION_H
#define MO_DIAGNOSTICSSTATUSNOTIFICATION_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/Diagnostics/Diagnostics.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16 && MO_ENABLE_DIAGNOSTICS

namespace MicroOcpp {
namespace v16 {

class DiagnosticsStatusNotification : public Operation, public MemoryManaged {
private:
    DiagnosticsStatus status = DiagnosticsStatus::Idle;
public:
    DiagnosticsStatusNotification(DiagnosticsStatus status);

    const char* getOperationType() override {return "DiagnosticsStatusNotification"; }

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;
};

} //namespace v16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16 && MO_ENABLE_DIAGNOSTICS
#endif
