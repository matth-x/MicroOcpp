// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef STARTTRANSACTION_H
#define STARTTRANSACTION_H

#include <ArduinoOcpp/Core/OcppMessage.h>

#include <Variants.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class StartTransaction : public OcppMessage {
private:
  int connectorId = 1;
  String idTag = String('\0');
public:
  StartTransaction(int connectorId);

  StartTransaction(int connectorId, String &idTag);

  const char* getOcppOperationType();

  DynamicJsonDocument* createReq();

  void processConf(JsonObject payload);

  void processReq(JsonObject payload);

  DynamicJsonDocument* createConf();
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
