// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Operations/GetDiagnostics.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Diagnostics/DiagnosticsService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::GetDiagnostics;

GetDiagnostics::GetDiagnostics(DiagnosticsService& diagService) : diagService(diagService) {

}

void GetDiagnostics::processReq(JsonObject payload) {

    const char *location = payload["location"] | "";
    //check location URL. Maybe introduce Same-Origin-Policy?
    if (!*location) {
        errorCode = "FormationViolation";
        return;
    }
    
    int retries = payload["retries"] | 1;
    int retryInterval = payload["retryInterval"] | 180;
    if (retries < 0 || retryInterval < 0) {
        errorCode = "PropertyConstraintViolation";
        return;
    }

    Timestamp startTime;
    if (payload.containsKey("startTime")) {
        if (!startTime.setTime(payload["startTime"] | "Invalid")) {
            errorCode = "PropertyConstraintViolation";
            MO_DBG_WARN("bad time format");
            return;
        }
    }

    Timestamp stopTime;
    if (payload.containsKey("startTime")) {
        if (!stopTime.setTime(payload["stopTime"] | "Invalid")) {
            errorCode = "PropertyConstraintViolation";
            MO_DBG_WARN("bad time format");
            return;
        }
    }

    fileName = diagService.requestDiagnosticsUpload(location, (unsigned int) retries, (unsigned int) retryInterval, startTime, stopTime);
}

std::unique_ptr<DynamicJsonDocument> GetDiagnostics::createConf(){
    if (fileName.empty()) {
        return createEmptyDocument();
    } else {
        auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1) + fileName.length() + 1));
        JsonObject payload = doc->to<JsonObject>();
        payload["fileName"] = fileName;
        return doc;
    }
}
