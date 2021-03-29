// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef RESET_H
#define RESET_H

#include <ArduinoOcpp/Core/OcppMessage.h>

#include <Variants.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class Reset : public OcppMessage {
public:
  Reset();

  const char* getOcppOperationType();

  void processReq(JsonObject payload);

  DynamicJsonDocument* createConf();
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
