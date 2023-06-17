// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef DIAGNOSTICSSERVICE_H
#define DIAGNOSTICSSERVICE_H

#include <functional>
#include <memory>
#include <ArduinoOcpp/Core/Time.h>
#include <ArduinoOcpp/Model/Diagnostics/DiagnosticsStatus.h>
#include <string>

namespace ArduinoOcpp {

enum class UploadStatus {
    NotUploaded,
    Uploaded,
    UploadFailed
};

class Context;
class Request;

class DiagnosticsService {
private:
    Context& context;
    
    std::string location {};
    int retries = 0;
    unsigned int retryInterval = 0;
    Timestamp startTime = Timestamp();
    Timestamp stopTime = Timestamp();

    Timestamp nextTry = Timestamp();

    std::function<bool(const std::string &location, Timestamp &startTime, Timestamp &stopTime)> onUpload = nullptr;
    std::function<UploadStatus()> uploadStatusSampler = nullptr;
    bool uploadIssued = false;

    std::unique_ptr<Request> getDiagnosticsStatusNotification();

    Ocpp16::DiagnosticsStatus lastReportedStatus = Ocpp16::DiagnosticsStatus::Idle;

public:
    DiagnosticsService(Context& context);

    void loop();

    //timestamps before year 2021 will be treated as "undefined"
    //returns empty std::string if onUpload is missing or upload cannot be scheduled for another reason
    //returns fileName of diagnostics file to be uploaded if upload has been scheduled
    std::string requestDiagnosticsUpload(const std::string &location, int retries = 1, unsigned int retryInterval = 0, Timestamp startTime = Timestamp(), Timestamp stopTime = Timestamp());

    Ocpp16::DiagnosticsStatus getDiagnosticsStatus();

    void setOnUpload(std::function<bool(const std::string &location, Timestamp &startTime, Timestamp &stopTime)> onUpload);

    void setOnUploadStatusSampler(std::function<UploadStatus()> uploadStatusSampler);
};

#if !defined(AO_CUSTOM_DIAGNOSTICS) && !defined(AO_CUSTOM_WS)
#if defined(ESP32) || defined(ESP8266)

namespace EspWiFi {

DiagnosticsService *makeDiagnosticsService(Context& context);

}

#endif //defined(ESP32) || defined(ESP8266)
#endif //!defined(AO_CUSTOM_UPDATER) && !defined(AO_CUSTOM_WS)

} //end namespace ArduinoOcpp

#endif
