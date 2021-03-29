// matth-x/ESP8266-OCPP
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
  SmartChargingService *smartChargingService;
public:
  SetChargingProfile(SmartChargingService *smartChargingService);

  const char* getOcppOperationType();

  void processReq(JsonObject payload);

  DynamicJsonDocument* createConf();
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
