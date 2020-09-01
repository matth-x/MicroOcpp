// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#ifndef AUTHORIZE_H
#define AUTHORIZE_H

#include "OcppMessage.h"

class Authorize : public OcppMessage {
private:
  String idTag;
public:
  Authorize();

  Authorize(String &idTag);

  const char* getOcppOperationType();

  DynamicJsonDocument* createReq();

  void processConf(JsonObject payload);

  void processReq(JsonObject payload);

  DynamicJsonDocument* createConf();

};

#endif
