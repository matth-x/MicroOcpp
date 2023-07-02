// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Operations/DiagnosticsStatusNotification.h>
#include <ArduinoOcpp/Core/Context.h>
#include <ArduinoOcpp/Model/Model.h>
#include <ArduinoOcpp/Model/Diagnostics/DiagnosticsService.h>

using ArduinoOcpp::Ocpp16::DiagnosticsStatusNotification;

DiagnosticsStatusNotification::DiagnosticsStatusNotification(DiagnosticsStatus status) : status(status) {
    
}

const char *DiagnosticsStatusNotification::cstrFromStatus(DiagnosticsStatus status) {
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
    return nullptr; //cannot be reached
}

std::unique_ptr<DynamicJsonDocument> DiagnosticsStatusNotification::createReq() {
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = cstrFromStatus(status);
    return doc;
}

void DiagnosticsStatusNotification::processConf(JsonObject payload){
    // no payload, nothing to do
}
