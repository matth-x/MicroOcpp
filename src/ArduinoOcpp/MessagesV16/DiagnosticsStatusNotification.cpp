// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/MessagesV16/DiagnosticsStatusNotification.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Tasks/Diagnostics/DiagnosticsService.h>

using ArduinoOcpp::Ocpp16::DiagnosticsStatusNotification;

DiagnosticsStatusNotification::DiagnosticsStatusNotification() {
    DiagnosticsService *diagService = getDiagnosticsService();
    if (diagService) {
        status = diagService->getDiagnosticsStatus();
    } else {
        status = DiagnosticsStatus::Idle;
    }
}

DiagnosticsStatusNotification::DiagnosticsStatusNotification(DiagnosticsStatus status) : status(status) {
}

const char *DiagnosticsStatusNotification::cstrFromFwStatus(DiagnosticsStatus status) {
    switch (status) {
        case (DiagnosticsStatus::Idle):
            return "Idle";
            break;
        case (DiagnosticsStatus::Uploaded):
            return "Uploaded";
            break;
        case (DiagnosticsStatus::UploadFailed):
            return "UploadFailed";
            break;
        case (DiagnosticsStatus::Uploading):
            return "Uploading";
            break;
    }
    return NULL; //cannot be reached
}

DynamicJsonDocument* DiagnosticsStatusNotification::createReq() {
  DynamicJsonDocument *doc = new DynamicJsonDocument(JSON_OBJECT_SIZE(1));
  JsonObject payload = doc->to<JsonObject>();
  payload["status"] = cstrFromFwStatus(status);
  return doc;
}

void DiagnosticsStatusNotification::processConf(JsonObject payload){
    // no payload, nothing to do
}
