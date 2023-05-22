// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CHANGEAVAILABILITY_H
#define CHANGEAVAILABILITY_H

#include <ArduinoOcpp/Core/OcppMessage.h>

namespace ArduinoOcpp {

class OcppModel;

namespace Ocpp16 {

class ChangeAvailability : public OcppMessage {
private:
    OcppModel& context;
    bool scheduled = false;
    bool accepted = false;

    const char *errorCode {nullptr};
public:
    ChangeAvailability(OcppModel& context);

    const char* getOcppOperationType();

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();

    const char *getErrorCode() {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
