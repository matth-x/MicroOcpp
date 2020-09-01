// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#ifndef BOOTNOTIFICATION_H
#define BOOTNOTIFICATION_H

#include "OcppMessage.h"

class BootNotification : public OcppMessage {
public:
  BootNotification();

  const char* getOcppOperationType();

  DynamicJsonDocument* createReq();

  void processConf(JsonObject payload);

  void processReq(JsonObject payload);

  DynamicJsonDocument* createConf();
};



#endif
