// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/MessagesV16/GetDiagnostics.h>
#include <ArduinoOcpp/Core/OcppEngine.h>

using ArduinoOcpp::Ocpp16::GetDiagnostics;

GetDiagnostics::GetDiagnostics() {

}

void GetDiagnostics::processReq(JsonObject payload) {
    /*
     * Process the application data here. Note: you have to implement the FW update procedure in your client code. You have to set
     * a onSendConfListener in which you initiate a FW update (e.g. calling ESPhttpUpdate.update(...) )
     */

    const char *loc = payload["location"] | "";
    location = String(loc);
    //check location URL. Maybe introduce Same-Origin-Policy?
    if (location.isEmpty()) {
        formatError = true;
        Serial.println(F("[GetDiagnostics] Could not read location. Abort"));
        return;
    }
    
    retries = payload["retries"] | 1;
    retryInterval = payload["retryInterval"] | 180;

    //check the integrity of startTime
    if (payload.containsKey("startTime")) {
        const char *startTimeRaw = payload["startTime"] | "Invalid";
        if (!startTime.setTime(startTimeRaw)) {
            formatError = true;
            Serial.println(F("[GetDiagnostics] Could not read startTime. Abort"));
            return;
        }
    }

    //check the integrity of stopTime
    if (payload.containsKey("startTime")) {
        const char *stopTimeRaw = payload["stopTime"] | "Invalid";
        if (!stopTime.setTime(stopTimeRaw)) {
            formatError = true;
            Serial.println(F("[GetDiagnostics] Could not read stopTime. Abort"));
            return;
        }
    }
}

DynamicJsonDocument* GetDiagnostics::createConf(){
    DiagnosticsService *diagService = getDiagnosticsService();
    if (diagService != NULL) {
        fileName = diagService->requestDiagnosticsUpload(location, retries, retryInterval, startTime, stopTime);
    } else {
        Serial.println(F("[GetDiagnostics] DiagnosticsService has not been initialized before! Please have a look at ArduinoOcpp.cpp for an example. Abort"));
    }

    if (fileName.isEmpty()) {
        DynamicJsonDocument* doc = new DynamicJsonDocument(0);
        doc->to<JsonObject>();
        return doc;
    } else {
        DynamicJsonDocument* doc = new DynamicJsonDocument(JSON_OBJECT_SIZE(1) + fileName.length() + 1);
        JsonObject payload = doc->to<JsonObject>();
        payload["fileName"] = fileName;
        return doc;
    }
}
