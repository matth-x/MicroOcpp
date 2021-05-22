// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef CHANGECONFIGURATION_H
#define CHANGECONFIGURATION_H

#include <Variants.h>

#include <ArduinoOcpp/Core/OcppMessage.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class ChangeConfiguration : public OcppMessage {
private:
  bool err = false;
  bool rebootRequired = false;
  bool readOnly = false;
public:
  ChangeConfiguration();

  const char* getOcppOperationType();

  void processReq(JsonObject payload);

  DynamicJsonDocument* createConf();

};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
