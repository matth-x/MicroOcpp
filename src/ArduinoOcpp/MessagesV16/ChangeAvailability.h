// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
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

    std::unique_ptr<DynamicJsonDocument> createConf();
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
