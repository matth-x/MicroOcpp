// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/Diagnostics/DiagnosticsStatus.h>

#ifndef MO_DIAGNOSTICSSTATUSNOTIFICATION_H
#define MO_DIAGNOSTICSSTATUSNOTIFICATION_H

namespace MicroOcpp {
namespace Ocpp16 {

class DiagnosticsStatusNotification : public Operation, public MemoryManaged {
private:
    DiagnosticsStatus status = DiagnosticsStatus::Idle;
    static const char *cstrFromStatus(DiagnosticsStatus status);
public:
    DiagnosticsStatusNotification(DiagnosticsStatus status);

    const char* getOperationType() override {return "DiagnosticsStatusNotification"; }

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

};

} //end namespace Ocpp16
} //end namespace MicroOcpp

#endif
