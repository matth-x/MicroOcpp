// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#ifndef CHANGECONFIGURATION_H
#define CHANGECONFIGURATION_H

#include <Variants.h>

#include "OcppMessage.h"

class ChangeConfiguration : public OcppMessage {
private:
  bool err = false;
  bool rebootRequired = false;
  bool readOnly = false;
public:
  ChangeConfiguration();

  const char* getOcppOperationType();

  void processReq(JsonObject payload);

  DynamicJsonDocument* createConf();

};

#endif
