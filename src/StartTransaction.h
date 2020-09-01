// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#ifndef STARTTRANSACTION_H
#define STARTTRANSACTION_H

#include "Variants.h"

#include "OcppMessage.h"

class StartTransaction : public OcppMessage {
private:
  String idTag = String('\0');
public:
  StartTransaction();

  StartTransaction(String &idTag);

  const char* getOcppOperationType();

  DynamicJsonDocument* createReq();

  void processConf(JsonObject payload);

  void processReq(JsonObject payload);

  DynamicJsonDocument* createConf();
};

#endif
