// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#ifndef REMOTESTARTTRANSACTION_H
#define REMOTESTARTTRANSACTION_H

#include "Variants.h"

#include "OcppMessage.h"

class RemoteStartTransaction : public OcppMessage {
private:
  String idTag;
  int connectorId;
public:
  RemoteStartTransaction();

  const char* getOcppOperationType();

  DynamicJsonDocument* createReq();

  void processConf(JsonObject payload);

  void processReq(JsonObject payload);

  DynamicJsonDocument* createConf();
};

#endif
