// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef TRIGGERMESSAGE_H
#define TRIGGERMESSAGE_H

#include <ArduinoOcpp/Core/OcppMessage.h>
#include <ArduinoOcpp/Core/OcppOperation.h>
#include <Variants.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class TriggerMessage : public OcppMessage {
private:
  OcppOperation *triggeredOperation;
  const char *statusMessage;
public:
  TriggerMessage();

  const char* getOcppOperationType();

  void processReq(JsonObject payload);

  DynamicJsonDocument* createConf();
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
