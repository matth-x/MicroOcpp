// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#ifndef STATUSNOTIFICATION_H
#define STATUSNOTIFICATION_H

#include "OcppMessage.h"
#include "ChargePointStatusService.h"
#include "TimeHelper.h"

class StatusNotification : public OcppMessage {
private:
  ChargePointStatus currentStatus = ChargePointStatus::NOT_SET;
  char timestamp[JSONDATE_LENGTH + 1] = {'\0'};
public:
  StatusNotification(ChargePointStatus currentStatus);

  StatusNotification();

  const char* getOcppOperationType();

  DynamicJsonDocument* createReq();

  void processConf(JsonObject payload);

  void processReq(JsonObject payload);

  DynamicJsonDocument* createConf();
};

#endif
