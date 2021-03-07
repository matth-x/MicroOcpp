// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef GETCONFIGURATION_H
#define GETCONFIGURATION_H

#include <Variants.h>

#include "OcppMessage.h"
#include "LinkedList.h"

class GetConfiguration : public OcppMessage {
private:
  LinkedList<String> keys;
public:
  GetConfiguration();
  ~GetConfiguration();

  const char* getOcppOperationType();

  void processReq(JsonObject payload);

  DynamicJsonDocument* createConf();

};
#endif
