// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef DIAGNOSTICSSERVICE_H
#define DIAGNOSTICSSERVICE_H

#include <Arduino.h>
#include <functional>
#include <ArduinoOcpp/Core/OcppTime.h>
#include <ArduinoOcpp/Core/OcppOperation.h>
#include <ArduinoOcpp/MessagesV16/DiagnosticsStatusNotification.h>

namespace ArduinoOcpp {

enum class UploadStatus {
    NotUploaded,
    Uploaded,
    UploadFailed
};

class DiagnosticsService {
private:
    String location = String('\0');
    int retries = 0;
    unsigned int retryInterval = 0;
    OcppTimestamp startTime = OcppTimestamp();
    OcppTimestamp stopTime = OcppTimestamp();

    OcppTimestamp nextTry = OcppTimestamp();

    std::function<bool(String &location, OcppTimestamp &startTime, OcppTimestamp &stopTime)> onUpload = NULL;
    std::function<UploadStatus()> uploadStatusSampler = NULL;
    bool uploadIssued = false;

    std::unique_ptr<OcppOperation> getDiagnosticsStatusNotification();

    Ocpp16::DiagnosticsStatus lastReportedStatus = Ocpp16::DiagnosticsStatus::Idle;

public:
    DiagnosticsService() = default;

    void loop();

    //timestamps before year 2021 will be treated as "undefined"
    //returns empty String if onUpload is missing or upload cannot be scheduled for another reason
    //returns fileName of diagnostics file to be uploaded if upload has been scheduled
    String requestDiagnosticsUpload(String &location, int retries = 1, unsigned int retryInterval = 0, OcppTimestamp startTime = OcppTimestamp(), OcppTimestamp stopTime = OcppTimestamp());

    Ocpp16::DiagnosticsStatus getDiagnosticsStatus();

    void setOnUpload(std::function<bool(String &location, OcppTimestamp &startTime, OcppTimestamp &stopTime)> onUpload);

    void setOnUploadStatusSampler(std::function<UploadStatus()> uploadStatusSampler);
};

#if !defined(AO_CUSTOM_DIAGNOSTICS) && !defined(AO_CUSTOM_WEBSOCKET)
#if defined(ESP32) || defined(ESP8266)

namespace EspWiFi {

DiagnosticsService *makeDiagnosticsService();

}

#endif //defined(ESP32) || defined(ESP8266)
#endif //!defined(AO_CUSTOM_UPDATER) && !defined(AO_CUSTOM_WEBSOCKET)

} //end namespace ArduinoOcpp

#endif
