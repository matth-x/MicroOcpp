// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Model/Diagnostics/DiagnosticsService.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>
#include <MicroOcpp/Debug.h>

#include <MicroOcpp/Operations/GetDiagnostics.h>
#include <MicroOcpp/Operations/DiagnosticsStatusNotification.h>

using namespace MicroOcpp;
using Ocpp16::DiagnosticsStatus;

DiagnosticsService::DiagnosticsService(Context& context) : context(context) {
    
    context.getOperationRegistry().registerOperation("GetDiagnostics", [this] () {
        return new Ocpp16::GetDiagnostics(*this);});

    //Register message handler for TriggerMessage operation
    context.getOperationRegistry().registerOperation("DiagnosticsStatusNotification", [this] () {
        return new Ocpp16::DiagnosticsStatusNotification(getDiagnosticsStatus());});
}

void DiagnosticsService::loop() {
    auto notification = getDiagnosticsStatusNotification();
    if (notification) {
        context.initiateRequest(std::move(notification));
    }

    const auto& timestampNow = context.getModel().getClock().now();
    if (retries > 0 && timestampNow >= nextTry) {

        if (!uploadIssued) {
            if (onUpload != nullptr) {
                MO_DBG_DEBUG("Call onUpload");
                onUpload(location.c_str(), startTime, stopTime);
                uploadIssued = true;
            } else {
                MO_DBG_ERR("onUpload must be set! (see setOnUpload). Will abort");
                retries = 0;
                uploadIssued = false;
            }
        }

        if (uploadIssued) {
            if (uploadStatusInput != nullptr && uploadStatusInput() == UploadStatus::Uploaded) {
                //success!
                MO_DBG_DEBUG("end upload routine (by status)")
                uploadIssued = false;
                retries = 0;
            }

            //check if maximum time elapsed or failed
            const int UPLOAD_TIMEOUT = 60;
            if (timestampNow - nextTry >= UPLOAD_TIMEOUT
                    || (uploadStatusInput != nullptr && uploadStatusInput() == UploadStatus::UploadFailed)) {
                //maximum upload time elapsed or failed

                if (uploadStatusInput == nullptr) {
                    //No way to find out if failed. But maximum time has elapsed. Assume success
                    MO_DBG_DEBUG("end upload routine (by timer)");
                    uploadIssued = false;
                    retries = 0;
                } else {
                    //either we have UploadFailed status or (NotDownloaded + timeout) here
                    MO_DBG_WARN("Upload timeout or failed");
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
std::string DiagnosticsService::requestDiagnosticsUpload(const char *location, unsigned int retries, unsigned int retryInterval, Timestamp startTime, Timestamp stopTime) {
    if (onUpload == nullptr) //maybe add further plausibility checks
        return std::string{};
    
    this->location = location;
    this->retries = retries;
    this->retryInterval = retryInterval;
    this->startTime = startTime;
    
    Timestamp stopMin = Timestamp(2021,0,0,0,0,0);
    if (stopTime >= stopMin) {
        this->stopTime = stopTime;
    } else {
        auto newStop = context.getModel().getClock().now();
        newStop += 3600 * 24 * 365; //set new stop time one year in future
        this->stopTime = newStop;
    }
    
#if MO_DBG_LEVEL >= MO_DL_INFO
    char dbuf [JSONDATE_LENGTH + 1] = {'\0'};
    char dbuf2 [JSONDATE_LENGTH + 1] = {'\0'};
    this->startTime.toJsonString(dbuf, JSONDATE_LENGTH + 1);
    this->stopTime.toJsonString(dbuf2, JSONDATE_LENGTH + 1);

    MO_DBG_INFO("Scheduled Diagnostics upload!\n" \
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
#endif

    nextTry = context.getModel().getClock().now();
    nextTry += 5; //wait for 5s before upload
    uploadIssued = false;

    nextTry.toJsonString(dbuf, JSONDATE_LENGTH + 1);
    MO_DBG_DEBUG("Initial try at %s", dbuf);

    std::string fileName;
    if (refreshFilename) {
        fileName = refreshFilename();
    } else {
        fileName = "diagnostics.log";
    }
    return fileName;
}

DiagnosticsStatus DiagnosticsService::getDiagnosticsStatus() {
    if (uploadIssued) {
        if (uploadStatusInput != nullptr) {
            switch (uploadStatusInput()) {
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

void DiagnosticsService::setRefreshFilename(std::function<std::string()> refreshFn) {
    this->refreshFilename = refreshFn;
}

void DiagnosticsService::setOnUpload(std::function<bool(const char *location, Timestamp &startTime, Timestamp &stopTime)> onUpload) {
    this->onUpload = onUpload;
}

void DiagnosticsService::setOnUploadStatusInput(std::function<UploadStatus()> uploadStatusInput) {
    this->uploadStatusInput = uploadStatusInput;
}

#if !defined(MO_CUSTOM_DIAGNOSTICS) && !defined(MO_CUSTOM_WS)
#if defined(ESP32) || defined(ESP8266)

DiagnosticsService *EspWiFi::makeDiagnosticsService(Context& context) {
    auto diagService = new DiagnosticsService(context);

    /*
     * add onUpload and uploadStatusInput when logging was implemented
     */

    return diagService;
}

#endif //defined(ESP32) || defined(ESP8266)
#endif //!defined(MO_CUSTOM_UPDATER) && !defined(MO_CUSTOM_WS)
