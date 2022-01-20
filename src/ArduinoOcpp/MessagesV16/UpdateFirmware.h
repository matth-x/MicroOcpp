// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef UPDATEFIRMWARE_H
#define UPDATEFIRMWARE_H

#include <ArduinoOcpp/Core/OcppMessage.h>
#include <ArduinoOcpp/Core/OcppTime.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class UpdateFirmware : public OcppMessage {
private:
  String location = String('\0');
  OcppTimestamp retreiveDate = OcppTimestamp();
  int retries = 1;
  ulong retryInterval = 180;
  bool formatError = false;
public:
  UpdateFirmware();

  const char* getOcppOperationType() {return "UpdateFirmware";}

  void processReq(JsonObject payload);

  std::unique_ptr<DynamicJsonDocument> createConf();

  const char *getErrorCode() {if (formatError) return "FormationViolation"; else return NULL;}
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp

#endif
