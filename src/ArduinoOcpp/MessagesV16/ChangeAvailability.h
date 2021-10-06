// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef CHANGEAVAILABILITY_H
#define CHANGEAVAILABILITY_H

#include <ArduinoOcpp/Core/OcppMessage.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class ChangeAvailability : public OcppMessage {
private:
    bool scheduled = false;
    bool accepted = false;
public:
    ChangeAvailability();

    const char* getOcppOperationType();

    void processReq(JsonObject payload);

    DynamicJsonDocument* createConf();
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
