// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef SETCHARGINGPROFILE_H
#define SETCHARGINGPROFILE_H

#include <ArduinoOcpp/Core/OcppMessage.h>
#include <ArduinoOcpp/Tasks/SmartCharging/SmartChargingService.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class SetChargingProfile : public OcppMessage {
private:
    SmartChargingService *smartChargingService = NULL;

    DynamicJsonDocument *payloadToClient = NULL;
public:
    SetChargingProfile(SmartChargingService *smartChargingService);

    SetChargingProfile(DynamicJsonDocument *payloadToClient);

    ~SetChargingProfile();

    const char* getOcppOperationType();

    void processReq(JsonObject payload);

    DynamicJsonDocument* createConf();

    DynamicJsonDocument* createReq();

    void processConf(JsonObject payload);
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
