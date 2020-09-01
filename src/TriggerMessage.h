// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#ifndef TRIGGERMESSAGE_H
#define TRIGGERMESSAGE_H

#include "Variants.h"

#include <WebSocketsClient.h>

#include "OcppMessage.h"
#include "OcppOperation.h"

class TriggerMessage : public OcppMessage {
private:
  WebSocketsClient *webSocket;
  OcppOperation *triggeredOperation;
  const char *statusMessage;
public:
  TriggerMessage(WebSocketsClient *webSocket);

  const char* getOcppOperationType();

  void processReq(JsonObject payload);

  DynamicJsonDocument* createConf();
};

#endif
