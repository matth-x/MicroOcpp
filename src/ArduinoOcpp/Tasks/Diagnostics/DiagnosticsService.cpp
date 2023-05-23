// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Tasks/Diagnostics/DiagnosticsService.h>
#include <ArduinoOcpp/Core/Context.h>
#include <ArduinoOcpp/Core/Model.h>
#include <ArduinoOcpp/Core/SimpleRequestFactory.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Debug.h>

#include <ArduinoOcpp/MessagesV16/GetDiagnostics.h>
#include <ArduinoOcpp/MessagesV16/DiagnosticsStatusNotification.h>

using namespace ArduinoOcpp;
using Ocpp16::DiagnosticsStatus;

DiagnosticsService::DiagnosticsService(Context& context) : context(context) {
    const char *fpId = "FirmwareManagement";
    auto fProfile = declareConfiguration<const char*>("SupportedFeatureProfiles",fpId, CONFIGURATION_VOLATILE, false, true, true, false);
    if (!strstr(*fProfile, fpId)) {
        auto fProfilePlus = std::string(*fProfile);
        if (!fProfilePlus.empty() && fProfilePlus.back() != ',')
            fProfilePlus += ",";
        fProfilePlus += fpId;
        fProfile->setValue(fProfilePlus.c_str(), fProfilePlus.length() + 1);
    }
    
    context.getOperationRegistry().registerOperation("GetDiagnostics", [&context] () {
        return new Ocpp16::GetDiagnostics(context.getModel());});

    //Register message handler for TriggerMessage operation
    context.getOperationRegistry().registerOperation("DiagnosticsStatusNotification", [this] () {
        return new Ocpp16::DiagnosticsStatusNotification(getDiagnosticsStatus());});
}

void DiagnosticsService::loop() {
    auto notification = getDiagnosticsStatusNotification();
    if (notification) {
        context.initiateRequest(std::move(notification));
    }

    const auto& timestampNow = context.getModel().getTime().getTimestampNow();
    if (retries > 0 && timestampNow >= nextTry) {

        if (!uploadIssued) {
            if (onUpload != nullptr) {
                AO_DBG_DEBUG("Call onUpload");
                onUpload(location, startTime, stopTime);
                uploadIssued = true;
            } else {
                AO_DBG_ERR("onUpload must be set! (see setOnUpload). Will abort");
                retries = 0;
                uploadIssued = false;
            }
        }

        if (uploadIssued) {
            if (uploadStatusSampler != nullptr && uploadStatusSampler() == UploadStatus::Uploaded) {
                //success!
                AO_DBG_DEBUG("end upload routine (by status)")
                uploadIssued = false;
                retries = 0;
            }

            //check if maximum time elapsed or failed
            const int UPLOAD_TIMEOUT = 60;
            if (timestampNow - nextTry >= UPLOAD_TIMEOUT
                    || (uploadStatusSampler != nullptr && uploadStatusSampler() == UploadStatus::UploadFailed)) {
                //maximum upload time elapsed or failed

                if (uploadStatusSampler == nullptr) {
                    //No way to find out if failed. But maximum time has elapsed. Assume success
                    AO_DBG_DEBUG("end upload routine (by timer)");
                    uploadIssued = false;
                    retries = 0;
                } else {
                    //either we have UploadFailed status or (NotDownloaded + timeout) here
                    AO_DBG_WARN("Upload timeout or failed");
                    const int TRANSITION_DELAY = 10;
                    if (retryInterval <= UPLOAD_TIMEOUT + TRANSITION_DELAY) {
                        nextTry = timestampNow;
                        nextTry += TRANSITION_DELAY; //wait for another 10 seconds
                    } else {
                        nextTry += retryInterval;
                    }
                    retries--;
                }
            }
        } //end if (uploadIssued)
    } //end try upload
}

//timestamps before year 2021 will be treated as "undefined"
std::string DiagnosticsService::requestDiagnosticsUpload(const std::string &location, int retries, unsigned int retryInterval, Timestamp startTime, Timestamp stopTime) {
    if (onUpload == nullptr) //maybe add further plausibility checks
        return nullptr;
    
    this->location = location;
    this->retries = retries;
    this->retryInterval = retryInterval;
    this->startTime = startTime;
    
    Timestamp stopMin = Timestamp(2021,0,0,0,0,0);
    if (stopTime >= stopMin) {
        this->stopTime = stopTime;
    } else {
        auto newStop = context.getModel().getTime().getTimestampNow();
        newStop += 3600 * 24 * 365; //set new stop time one year in future
        this->stopTime = newStop;
    }
    
    char dbuf [JSONDATE_LENGTH + 1] = {'\0'};
    char dbuf2 [JSONDATE_LENGTH + 1] = {'\0'};
    this->startTime.toJsonString(dbuf, JSONDATE_LENGTH + 1);
    this->stopTime.toJsonString(dbuf2, JSONDATE_LENGTH + 1);

    AO_DBG_INFO("Scheduled Diagnostics upload!\n" \
                    "                  location = %s\n" \
                    "                  retries = %i" \
                    ", retryInterval = %u" \
                    "                  startTime = %s\n" \
                    "                  stopTime = %s",
            this->location.c_str(),
            this->retries,
            this->retryInterval,
            dbuf,
            dbuf2);

    nextTry = context.getModel().getTime().getTimestampNow();
    nextTry += 5; //wait for 5s before upload
    uploadIssued = false;

    nextTry.toJsonString(dbuf, JSONDATE_LENGTH + 1);
    AO_DBG_DEBUG("Initial try at %s", dbuf);

    return "diagnostics.log";
}


DiagnosticsStatus DiagnosticsService::getDiagnosticsStatus() {
    if (uploadIssued) {
        if (uploadStatusSampler != nullptr) {
            switch (uploadStatusSampler()) {
                case UploadStatus::NotUploaded:
                    return DiagnosticsStatus::Uploading;
                case UploadStatus::Uploaded:
                    return DiagnosticsStatus::Uploaded;
                case UploadStatus::UploadFailed:
                    return DiagnosticsStatus::UploadFailed;
            }
        }
        return DiagnosticsStatus::Uploading;
    }
    return DiagnosticsStatus::Idle;
}

std::unique_ptr<Request> DiagnosticsService::getDiagnosticsStatusNotification() {

    if (getDiagnosticsStatus() != lastReportedStatus) {
        lastReportedStatus = getDiagnosticsStatus();
        if (lastReportedStatus != DiagnosticsStatus::Idle) {
            Operation *diagNotificationMsg = new Ocpp16::DiagnosticsStatusNotification(lastReportedStatus);
            auto diagNotification = makeRequest(diagNotificationMsg);
            return diagNotification;
        }
    }

    return nullptr;
}

void DiagnosticsService::setOnUpload(std::function<bool(const std::string &location, Timestamp &startTime, Timestamp &stopTime)> onUpload) {
    this->onUpload = onUpload;
}

void DiagnosticsService::setOnUploadStatusSampler(std::function<UploadStatus()> uploadStatusSampler) {
    this->uploadStatusSampler = uploadStatusSampler;
}

#if !defined(AO_CUSTOM_DIAGNOSTICS) && !defined(AO_CUSTOM_WS)
#if defined(ESP32) || defined(ESP8266)

DiagnosticsService *EspWiFi::makeDiagnosticsService(Context& context) {
    auto diagService = new DiagnosticsService(context);

    /*
     * add onUpload and uploadStatusSampler when logging was implemented
     */

    return diagService;
}

#endif //defined(ESP32) || defined(ESP8266)
#endif //!defined(AO_CUSTOM_UPDATER) && !defined(AO_CUSTOM_WS)
