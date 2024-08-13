// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/DiagnosticsStatusNotification.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Diagnostics/DiagnosticsService.h>

using MicroOcpp::Ocpp16::DiagnosticsStatusNotification;
using MicroOcpp::JsonDoc;

DiagnosticsStatusNotification::DiagnosticsStatusNotification(DiagnosticsStatus status) : MemoryManaged("v16.Operation.", "DiagnosticsStatusNotification"), status(status) {
    
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

std::unique_ptr<JsonDoc> DiagnosticsStatusNotification::createReq() {
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = cstrFromStatus(status);
    return doc;
}

void DiagnosticsStatusNotification::processConf(JsonObject payload){
    // no payload, nothing to do
}
