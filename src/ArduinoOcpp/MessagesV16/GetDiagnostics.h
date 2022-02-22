// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef GETDIAGNOSTICS_H
#define GETDIAGNOSTICS_H

#include <ArduinoOcpp/Core/OcppMessage.h>
#include <ArduinoOcpp/Core/OcppTime.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class GetDiagnostics : public OcppMessage {
private:
    std::string location {};
    int retries = 1;
    ulong retryInterval = 180;
    OcppTimestamp startTime = OcppTimestamp();
    OcppTimestamp stopTime = OcppTimestamp();

    std::string fileName {};
    
    bool formatError = false;
public:
    GetDiagnostics();

    const char* getOcppOperationType() {return "GetDiagnostics";}

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();

    const char *getErrorCode() {return formatError ? "FormationViolation" : nullptr;}
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp

#endif
