// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Core/Operation.h>
#include <ArduinoOcpp/Tasks/Diagnostics/DiagnosticsStatus.h>

#ifndef DIAGNOSTICSSTATUSNOTIFICATION_H
#define DIAGNOSTICSSTATUSNOTIFICATION_H

namespace ArduinoOcpp {
namespace Ocpp16 {

class DiagnosticsStatusNotification : public Operation {
private:
    DiagnosticsStatus status = DiagnosticsStatus::Idle;
    static const char *cstrFromStatus(DiagnosticsStatus status);
public:
    DiagnosticsStatusNotification(DiagnosticsStatus status);

    const char* getOperationType() {return "DiagnosticsStatusNotification"; }

    std::unique_ptr<DynamicJsonDocument> createReq();

    void processConf(JsonObject payload);

};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp

#endif
