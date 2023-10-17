// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef DIAGNOSTICSSERVICE_H
#define DIAGNOSTICSSERVICE_H

#include <functional>
#include <memory>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Model/Diagnostics/DiagnosticsStatus.h>
#include <string>

namespace MicroOcpp {

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
    
    std::string location;
    unsigned int retries = 0;
    unsigned int retryInterval = 0;
    Timestamp startTime;
    Timestamp stopTime;

    Timestamp nextTry;

    std::function<std::string()> refreshFilename;
    std::function<bool(const char *location, Timestamp &startTime, Timestamp &stopTime)> onUpload;
    std::function<UploadStatus()> uploadStatusInput;
    bool uploadIssued = false;

    std::unique_ptr<Request> getDiagnosticsStatusNotification();

    Ocpp16::DiagnosticsStatus lastReportedStatus = Ocpp16::DiagnosticsStatus::Idle;

public:
    DiagnosticsService(Context& context);

    void loop();

    //timestamps before year 2021 will be treated as "undefined"
    //returns empty std::string if onUpload is missing or upload cannot be scheduled for another reason
    //returns fileName of diagnostics file to be uploaded if upload has been scheduled
    std::string requestDiagnosticsUpload(const char *location, unsigned int retries = 1, unsigned int retryInterval = 0, Timestamp startTime = Timestamp(), Timestamp stopTime = Timestamp());

    Ocpp16::DiagnosticsStatus getDiagnosticsStatus();

    void setRefreshFilename(std::function<std::string()> refreshFn); //refresh a new filename which will be used for the subsequent upload tries

    void setOnUpload(std::function<bool(const char *location, Timestamp &startTime, Timestamp &stopTime)> onUpload);

    void setOnUploadStatusInput(std::function<UploadStatus()> uploadStatusInput);
};

#if !defined(MO_CUSTOM_DIAGNOSTICS) && !defined(MO_CUSTOM_WS)
#if defined(ESP32) || defined(ESP8266)

namespace EspWiFi {

DiagnosticsService *makeDiagnosticsService(Context& context);

}

#endif //defined(ESP32) || defined(ESP8266)
#endif //!defined(MO_CUSTOM_UPDATER) && !defined(MO_CUSTOM_WS)

} //end namespace MicroOcpp

#endif
