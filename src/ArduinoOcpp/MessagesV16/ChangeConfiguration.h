// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef CHANGECONFIGURATION_H
#define CHANGECONFIGURATION_H

#include <ArduinoOcpp/Core/OcppMessage.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class ChangeConfiguration : public OcppMessage {
private:
    bool reject = false;
    bool rebootRequired = false;
    bool readOnly = false;
    bool notSupported = false;

    const char *errorCode = nullptr;
public:
    ChangeConfiguration();

    const char* getOcppOperationType();

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();

    const char *getErrorCode() {return errorCode;}

};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
