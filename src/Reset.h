// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#ifndef RESET_H
#define RESET_H

#include "Variants.h"

#include "OcppMessage.h"

class Reset : public OcppMessage {
public:
  Reset();

  const char* getOcppOperationType();

  void processReq(JsonObject payload);

  DynamicJsonDocument* createConf();
};

#endif
