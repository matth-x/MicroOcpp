// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Core/OcppMessage.h>
#include <ArduinoOcpp/Tasks/Diagnostics/DiagnosticsStatus.h>

#ifndef DIAGNOSTICSSTATUSNOTIFICATION_H
#define DIAGNOSTICSSTATUSNOTIFICATION_H

namespace ArduinoOcpp {
namespace Ocpp16 {

class DiagnosticsStatusNotification : public OcppMessage {
private:
    DiagnosticsStatus status;
    static const char *cstrFromStatus(DiagnosticsStatus status);
public:
    DiagnosticsStatusNotification();

    DiagnosticsStatusNotification(DiagnosticsStatus status);

    const char* getOcppOperationType() {return "DiagnosticsStatusNotification"; }

    std::unique_ptr<DynamicJsonDocument> createReq();

    void processConf(JsonObject payload);

};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp

#endif
