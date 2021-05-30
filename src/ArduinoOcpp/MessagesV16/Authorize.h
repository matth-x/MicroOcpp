// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef AUTHORIZE_H
#define AUTHORIZE_H

#include <ArduinoOcpp/Core/OcppMessage.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

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

} //end namespace Ocpp16
} //end namespace ArduinoOcpp

#endif
