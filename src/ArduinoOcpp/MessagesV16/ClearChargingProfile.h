// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef CLEARCHARGINGPROFILE_H
#define CLEARCHARGINGPROFILE_H

#include <ArduinoOcpp/Core/OcppMessage.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class ClearChargingProfile : public OcppMessage {
private:
    bool matchingProfilesFound = false;
public:
    ClearChargingProfile();

    const char* getOcppOperationType();

    void processReq(JsonObject payload);

    DynamicJsonDocument* createConf();
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
