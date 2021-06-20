// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef UPDATEFIRMWARE_H
#define UPDATEFIRMWARE_H

#include <ArduinoOcpp/Core/OcppMessage.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class UpdateFirmware : public OcppMessage {
private:
  const char *location = "Invalid";
  bool formatError = false;
public:
  UpdateFirmware();

  const char* getOcppOperationType() {return "UpdateFirmware";}

  void processReq(JsonObject payload);

  DynamicJsonDocument* createConf();

  const char *getErrorCode() {if (formatError) return "FormationViolation"; else return NULL;}
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp

#endif
