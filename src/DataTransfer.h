// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#ifndef DATATRANSFER_H
#define DATATRANSFER_H

#include "OcppMessage.h"

class DataTransfer : public OcppMessage {
private:
  String msg;
public:
  DataTransfer(String &msg);

  const char* getOcppOperationType();

  DynamicJsonDocument* createReq();

  void processConf(JsonObject payload);

};

#endif
