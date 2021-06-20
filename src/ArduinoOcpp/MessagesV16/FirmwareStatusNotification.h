// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/Core/OcppMessage.h>

#ifndef FIRMWARESTATUSNOTIFICATION_H
#define FIRMWARESTATUSNOTIFICATION_H

namespace ArduinoOcpp {
namespace Ocpp16 {

class FirmwareStatusNotification : public OcppMessage {
private:
  String status;
public:
  FirmwareStatusNotification();

  FirmwareStatusNotification(String &status);

  FirmwareStatusNotification(const char *status);

  const char* getOcppOperationType() {return "FirmwareStatusNotification"; }

  DynamicJsonDocument* createReq();

  void processConf(JsonObject payload);

};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp

#endif
