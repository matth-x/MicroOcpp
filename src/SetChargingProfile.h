// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#ifndef SETCHARGINGPROFILE_H
#define SETCHARGINGPROFILE_H

#include "OcppMessage.h"
#include "SmartChargingService.h"

class SetChargingProfile : public OcppMessage {
private:
  SmartChargingService *smartChargingService;
public:
  SetChargingProfile(SmartChargingService *smartChargingService);

  const char* getOcppOperationType();

  void processReq(JsonObject payload);

  DynamicJsonDocument* createConf();
};



#endif
