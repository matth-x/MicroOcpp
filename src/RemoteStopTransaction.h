// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#ifndef REMOTESTOPTRANSACTION_H
#define REMOTESTOPTRANSACTION_H

#include "Variants.h"

#include "OcppMessage.h"

class RemoteStopTransaction : public OcppMessage {
private:
  int transactionId;
public:
  RemoteStopTransaction();

  const char* getOcppOperationType();

  void processReq(JsonObject payload);

  DynamicJsonDocument* createConf();
};

#endif
