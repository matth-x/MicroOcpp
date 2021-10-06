// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/Core/OcppMessage.h>

#ifndef FIRMWARESTATUSNOTIFICATION_H
#define FIRMWARESTATUSNOTIFICATION_H

namespace ArduinoOcpp {
namespace Ocpp16 {

enum class FirmwareStatus {
    Downloaded,
    DownloadFailed,
    Downloading,
    Idle,
    InstallationFailed,
    Installing,
    Installed
};

class FirmwareStatusNotification : public OcppMessage {
private:
  FirmwareStatus status;
  static const char *cstrFromFwStatus(FirmwareStatus status);
public:
  FirmwareStatusNotification();

  FirmwareStatusNotification(FirmwareStatus status);

  const char* getOcppOperationType() {return "FirmwareStatusNotification"; }

  DynamicJsonDocument* createReq();

  void processConf(JsonObject payload);

};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp

#endif
