// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/Diagnostics/DiagnosticsStatus.h>

#ifndef DIAGNOSTICSSTATUSNOTIFICATION_H
#define DIAGNOSTICSSTATUSNOTIFICATION_H

namespace MicroOcpp {
namespace Ocpp16 {

class DiagnosticsStatusNotification : public Operation {
private:
    DiagnosticsStatus status = DiagnosticsStatus::Idle;
    static const char *cstrFromStatus(DiagnosticsStatus status);
public:
    DiagnosticsStatusNotification(DiagnosticsStatus status);

    const char* getOperationType() override {return "DiagnosticsStatusNotification"; }

    std::unique_ptr<DynamicJsonDocument> createReq() override;

    void processConf(JsonObject payload) override;

};

} //end namespace Ocpp16
} //end namespace MicroOcpp

#endif
