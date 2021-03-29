// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef HEARTBEAT_H
#define HEARTBEAT_H

#include <ArduinoOcpp/Core/OcppMessage.h>

#include <Variants.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class Heartbeat : public OcppMessage {
public:
  Heartbeat();

  const char* getOcppOperationType();

  DynamicJsonDocument* createReq();

  void processConf(JsonObject payload);

  void processReq(JsonObject payload);

  DynamicJsonDocument* createConf();
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
