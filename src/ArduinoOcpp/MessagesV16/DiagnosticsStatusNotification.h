// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/Core/OcppMessage.h>

#ifndef DIAGNOSTICSSTATUSNOTIFICATION_H
#define DIAGNOSTICSSTATUSNOTIFICATION_H

namespace ArduinoOcpp {
namespace Ocpp16 {

enum class DiagnosticsStatus {
    Idle,
    Uploaded,
    UploadFailed,
    Uploading
};

class DiagnosticsStatusNotification : public OcppMessage {
private:
  DiagnosticsStatus status;
  static const char *cstrFromFwStatus(DiagnosticsStatus status);
public:
  DiagnosticsStatusNotification();

  DiagnosticsStatusNotification(DiagnosticsStatus status);

  const char* getOcppOperationType() {return "DiagnosticsStatusNotification"; }

  DynamicJsonDocument* createReq();

  void processConf(JsonObject payload);

};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp

#endif
