// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/Tasks/Diagnostics/DiagnosticsService.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>

using namespace ArduinoOcpp;
using Ocpp16::DiagnosticsStatus;

void DiagnosticsService::loop() {
    OcppOperation *notification = getDiagnosticsStatusNotification();
    if (notification) {
        initiateOcppOperation(notification);
    }

    const OcppTimestamp& timestampNow = getOcppTime()->getOcppTimestampNow();
    if (retries > 0 && timestampNow >= nextTry) {

        if (!uploadIssued) {
            if (onUpload != NULL) {
                onUpload(location, startTime, stopTime);
                uploadIssued = true;
            } else {
                Serial.println(F("[DiagnosticsService] onUpload must be set! (see setOnUpload). Will abort"));
                retries = 0;
                uploadIssued = false;
            }
        }

        if (uploadIssued) {
            if (uploadStatusSampler != NULL && uploadStatusSampler() == UploadStatus::Uploaded) {
                //success!
                uploadIssued = false;
                retries = 0;
            }

            //check if maximum time elapsed or failed
            const int UPLOAD_TIMEOUT = 60;
            if (timestampNow - nextTry >= UPLOAD_TIMEOUT
                    || (uploadStatusSampler != NULL && uploadStatusSampler() == UploadStatus::UploadFailed)) {
                //maximum upload time elapsed or failed

                if (uploadStatusSampler == NULL) {
                    //No way to find out if failed. But maximum time has elapsed. Assume success
                    uploadIssued = false;
                    retries = 0;
                } else {
                    //either we have UploadFailed status or (NotDownloaded + timeout) here
                    Serial.println(F("[DiagnosticsService] Upload timeout or failed!"));
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
String DiagnosticsService::requestDiagnosticsUpload(String &location, int retries, unsigned int retryInterval, OcppTimestamp startTime, OcppTimestamp stopTime) {
    if (onUpload == NULL) //maybe add further plausibility checks
        return String('\0');
    
    this->location = location;
    this->retries = retries;
    this->retryInterval = retryInterval;
    this->startTime = startTime;
    
    OcppTimestamp stopMin = OcppTimestamp(2021,0,0,0,0,0);
    if (stopTime >= stopMin) {
        this->stopTime = stopTime;
    } else {
        OcppTimestamp newStop = getOcppTime()->getOcppTimestampNow();
        newStop += 3600 * 24 * 365; //set new stop time one year in future
        this->stopTime = newStop;
    }
    
    if (DEBUG_OUT) {
        Serial.print(F("[DiagnosticsService] Scheduled Diagnostics upload!\n"));
        Serial.print(F("                     location = "));
        Serial.println(this->location);
        Serial.print(F("                     retries = "));
        Serial.print(this->retries);
        Serial.print(F(", retryInterval = "));
        Serial.println(this->retryInterval);
        Serial.print(F("                     startTime = "));
        char dbuf [JSONDATE_LENGTH + 1] = {'\0'};
        this->startTime.toJsonString(dbuf, JSONDATE_LENGTH + 1);
        Serial.print(F("                     stopTime = "));
        this->stopTime.toJsonString(dbuf, JSONDATE_LENGTH + 1);
        Serial.println(dbuf);
    }

    nextTry = getOcppTime()->getOcppTimestampNow();
    nextTry += 10; //wait for 10s before upload
    uploadIssued = false;

    return "diagnostics.log";
}


DiagnosticsStatus DiagnosticsService::getDiagnosticsStatus() {
    if (uploadIssued) {
        if (uploadStatusSampler != NULL) {
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

OcppOperation *DiagnosticsService::getDiagnosticsStatusNotification() {

    if (getDiagnosticsStatus() != lastReportedStatus) {
        lastReportedStatus = getDiagnosticsStatus();
        if (lastReportedStatus != DiagnosticsStatus::Idle) {
            OcppMessage *diagNotificationMsg = new Ocpp16::DiagnosticsStatusNotification(lastReportedStatus);
            OcppOperation *diagNotification = makeOcppOperation(diagNotificationMsg);
            return diagNotification;
        }
    }

    return NULL;
}

void DiagnosticsService::setOnUpload(std::function<bool(String &location, OcppTimestamp &startTime, OcppTimestamp &stopTime)> onUpload) {
    this->onUpload = onUpload;
}

void DiagnosticsService::setOnUploadStatusSampler(std::function<UploadStatus()> uploadStatusSampler) {
    this->uploadStatusSampler = uploadStatusSampler;
}

#if !defined(AO_CUSTOM_DIAGNOSTICS) && !defined(AO_CUSTOM_WEBSOCKET)
#if defined(ESP32) || defined(ESP8266)

DiagnosticsService *EspWiFi::makeDiagnosticsService() {
    DiagnosticsService *diagService = new DiagnosticsService();

    /*
     * add onUpload and uploadStatusSampler when logging was implemented
     */

    return diagService;
}

#endif //defined(ESP32) || defined(ESP8266)
#endif //!defined(AO_CUSTOM_UPDATER) && !defined(AO_CUSTOM_WEBSOCKET)
