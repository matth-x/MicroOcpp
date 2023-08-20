// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Operations/GetDiagnostics.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Diagnostics/DiagnosticsService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::GetDiagnostics;

GetDiagnostics::GetDiagnostics(Model& model) : model(model) {

}

void GetDiagnostics::processReq(JsonObject payload) {
    /*
     * Process the application data here. Note: you have to implement the FW update procedure in your client code. You have to set
     * a onSendConfListener in which you initiate a FW update (e.g. calling ESPhttpUpdate.update(...) )
     */

    const char *loc = payload["location"] | "";
    location = loc;
    //check location URL. Maybe introduce Same-Origin-Policy?
    if (location.empty()) {
        formatError = true;
        MOCPP_DBG_WARN("Could not read location. Abort");
        return;
    }
    
    retries = payload["retries"] | 1;
    retryInterval = payload["retryInterval"] | 180;

    //check the integrity of startTime
    if (payload.containsKey("startTime")) {
        const char *startTimeRaw = payload["startTime"] | "Invalid";
        if (!startTime.setTime(startTimeRaw)) {
            formatError = true;
            MOCPP_DBG_WARN("Could not read startTime. Abort");
            return;
        }
    }

    //check the integrity of stopTime
    if (payload.containsKey("startTime")) {
        const char *stopTimeRaw = payload["stopTime"] | "Invalid";
        if (!stopTime.setTime(stopTimeRaw)) {
            formatError = true;
            MOCPP_DBG_WARN("Could not read stopTime. Abort");
            return;
        }
    }
}

std::unique_ptr<DynamicJsonDocument> GetDiagnostics::createConf(){
    if (auto diagService = model.getDiagnosticsService()) {
        fileName = diagService->requestDiagnosticsUpload(location, retries, retryInterval, startTime, stopTime);
    } else {
        MOCPP_DBG_WARN("DiagnosticsService has not been initialized before! Please have a look at MicroOcpp.cpp for an example. Abort");
        return createEmptyDocument();
    }

    if (fileName.empty()) {
        return createEmptyDocument();
    } else {
        auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1) + fileName.length() + 1));
        JsonObject payload = doc->to<JsonObject>();
        payload["fileName"] = fileName;
        return doc;
    }
}
