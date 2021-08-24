// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/Tasks/FirmwareManagement/FirmwareService.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>
#include <ArduinoOcpp/MessagesV16/FirmwareStatusNotification.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>

using namespace ArduinoOcpp;
using ArduinoOcpp::Ocpp16::FirmwareStatus;

FirmwareService::FirmwareService(const char *buildNumber) : buildNumber(buildNumber) {
    previousBuildNumber = declareConfiguration<const char*>("BUILD_NUMBER", "", CONFIGURATION_FN, false, false, true, false);
}

void FirmwareService::loop() {
    OcppOperation *notification = getFirmwareStatusNotification();
    if (notification) {
        initiateOcppOperation(notification);
    }

    if (millis() - timestampTransition < delayTransition) {
        return;
    }

    FirmwareStatus status = getFirmwareStatus();
    OcppTimestamp timestampNow = getOcppTime()->getOcppTimestampNow();
    if (retries > 0 && timestampNow >= retreiveDate) {

        if (!downloadIssued) {
            downloadIssued = true;
            if (onDownload != NULL) {
                onDownload(location);
                timestampTransition = millis();
                delayTransition = 30000;
            }
            return;
        }

        const int DOWNLOAD_TIMEOUT = 120;
        if ((downloadIssued && timestampNow - retreiveDate >= DOWNLOAD_TIMEOUT)
                || (downloadStatusSampler != NULL && downloadStatusSampler() == DownloadStatus::DownloadFailed)) {
            
            Serial.println(F("[FirmwareService] Download timeout or failed! Retry"));
            if (retryInterval < DOWNLOAD_TIMEOUT)
                retreiveDate = timestampNow;
            else
                retreiveDate += retryInterval;
            retries--;
            downloadIssued = false;
            installationIssued = false;

            timestampTransition = millis();
            delayTransition = 10000;
            return;
        }

        if (downloadStatusSampler != NULL && downloadStatusSampler() == DownloadStatus::NotDownloaded) {
            return;
        }

        if (!installationIssued) {
            bool ongoingTx = false;
            ChargePointStatusService *cpStatus = getChargePointStatusService();
            if (cpStatus) {
                for (int i = 0; i < cpStatus->getNumConnectors(); i++) {
                    ConnectorStatus *connector = cpStatus->getConnector(i);
                    if (connector && connector->getTransactionId() >= 0) {
                        ongoingTx = true;
                        break;
                    }
                }
            }

            if (!ongoingTx) {
                installationIssued = true;

                if (onInstall != NULL) {
                    onInstall(location);
                } else {
                    Serial.println(F("[FirmwareService] onInstall must be set! (see setOnInstall). Will abort"));
                }

                timestampTransition = millis();
                delayTransition = 20000;
            }

            return;
        }

        const int INSTALLATION_TIMEOUT = 120;
        if ((installationIssued && timestampNow - retreiveDate >= INSTALLATION_TIMEOUT + DOWNLOAD_TIMEOUT)
                || (installationStatusSampler != NULL && installationStatusSampler() == InstallationStatus::InstallationFailed)) {

            Serial.println(F("[FirmwareService] Installation timeout or failed! Retry"));
            if (retryInterval < INSTALLATION_TIMEOUT + DOWNLOAD_TIMEOUT)
                retreiveDate = timestampNow;
            else
                retreiveDate += retryInterval;
            retries--;
            downloadIssued = false;
            installationIssued = false;

            timestampTransition = millis();
            delayTransition = 10000;
            return;
        }
        
        if (installationStatusSampler != NULL && installationStatusSampler() == InstallationStatus::NotInstalled) {
            return;
        }
        
    }
}

void FirmwareService::scheduleFirmwareUpdate(String &location, OcppTimestamp retreiveDate, int retries, ulong retryInterval) {
    this->location = String(location);
    this->retreiveDate = retreiveDate;
    this->retries = retries;
    this->retryInterval = retryInterval;

    downloadIssued = false;
    installationIssued = false;
}

FirmwareStatus FirmwareService::getFirmwareStatus() {
    if (installationIssued) {
        if (installationStatusSampler != NULL) {
            if (installationStatusSampler() == InstallationStatus::Installed) {
                return FirmwareStatus::Installed;
            } else if (installationStatusSampler() == InstallationStatus::InstallationFailed) {
                return FirmwareStatus::InstallationFailed;
            }
        }
        if (onInstall != NULL)
            return FirmwareStatus::Installing;
    }
    
    if (downloadIssued) {
        if (downloadStatusSampler != NULL) {
            if (downloadStatusSampler() == DownloadStatus::Downloaded) {
                return FirmwareStatus::Downloaded;
            } else if (downloadStatusSampler() == DownloadStatus::DownloadFailed) {
                return FirmwareStatus::DownloadFailed;
            }
        }
        if (onDownload != NULL)
            return FirmwareStatus::Downloading;
    }

    return FirmwareStatus::Idle;
}

OcppOperation *FirmwareService::getFirmwareStatusNotification() {
    /*
     * Check if FW has been updated previously
     */
    size_t buildNoSize = previousBuildNumber->getBuffsize();
    if (!strncmp(buildNumber, *previousBuildNumber, buildNoSize)) {
        //new FW
        previousBuildNumber->setValue(buildNumber, strlen(buildNumber));

        lastReportedStatus = FirmwareStatus::Installed;
        OcppMessage *fwNotificationMsg = new Ocpp16::FirmwareStatusNotification(lastReportedStatus);
        OcppOperation *fwNotification = makeOcppOperation(fwNotificationMsg);
        return fwNotification;
    }

    if (getFirmwareStatus() != lastReportedStatus) {
        lastReportedStatus = getFirmwareStatus();
        if (lastReportedStatus != FirmwareStatus::Idle) {
            OcppMessage *fwNotificationMsg = new Ocpp16::FirmwareStatusNotification(lastReportedStatus);
            OcppOperation *fwNotification = makeOcppOperation(fwNotificationMsg);
            return fwNotification;
        }
    }

    return NULL;
}

void FirmwareService::setOnDownload(std::function<bool(String &location)> onDownload) {
    this->onDownload = onDownload;
}

void FirmwareService::setDownloadStatusSampler(std::function<DownloadStatus()> downloadStatusSampler) {
    this->downloadStatusSampler = downloadStatusSampler;
}

void FirmwareService::setOnInstall(std::function<bool(String &location)> onInstall) {
    this->onInstall = onInstall;
}

void FirmwareService::setInstallationStatusSampler(std::function<InstallationStatus()> installationStatusSampler) {
    this->installationStatusSampler = installationStatusSampler;
}

#if !defined(AO_CUSTOM_UPDATER) && !defined(AO_CUSTOM_WEBSOCKET)
#if defined(ESP32)

#include <HTTPUpdate.h>

FirmwareService *makeFirmwareService(const char *buildNumber) {
    FirmwareService *fwService = new FirmwareService(buildNumber);

    fwService->setOnInstall([fwService = fwService] (String &location) {

        fwService->setInstallationStatusSampler([](){return InstallationStatus::NotInstalled;});

        WiFiClientSecure client;
        //client.setCACert(rootCACertificate);
        client.setTimeout(60); //in seconds
        
        // httpUpdate.setLedPin(LED_BUILTIN, HIGH);
        t_httpUpdate_return ret = httpUpdate.update(client, location.c_str());

        switch (ret) {
            case HTTP_UPDATE_FAILED:
                fwService->setInstallationStatusSampler([](){return InstallationStatus::InstallationFailed;});
                Serial.printf("[main] HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
                break;
            case HTTP_UPDATE_NO_UPDATES:
                fwService->setInstallationStatusSampler([](){return InstallationStatus::InstallationFailed;});
                Serial.println(F("[main] HTTP_UPDATE_NO_UPDATES"));
                break;
            case HTTP_UPDATE_OK:
                fwService->setInstallationStatusSampler([](){return InstallationStatus::Installed;});
                Serial.println(F("[main] HTTP_UPDATE_OK"));
                ESP.restart();
                break;
        }

        return true;
    });

    fwService->setInstallationStatusSampler([fwService = fwService] () {
        return InstallationStatus::NotInstalled;
    });

    return fwService;
}

#endif //defined(ESP32)
#if defined(ESP8266)

#include <ESP8266httpUpdate.h>

FirmwareService *makeFirmwareService(const char *buildNumber) {
    FirmwareService *fwService = new FirmwareService(buildNumber);

    fwService->setOnInstall([fwService = fwService] (String &location) {

        WiFiClientSecure client;
        //client.setCACert(rootCACertificate);
        client.setTimeout(60); //in seconds

        //ESPhttpUpdate.setLedPin(downloadStatusLedPin);

        HTTPUpdateResult ret = ESPhttpUpdate.update(client, location.c_str());

        switch (ret) {
            case HTTP_UPDATE_FAILED:
                fwService->setInstallationStatusSampler([](){return InstallationStatus::InstallationFailed;});
                Serial.printf("[main] HTTP_UPDATE_FAILED Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                break;
            case HTTP_UPDATE_NO_UPDATES:
                fwService->setInstallationStatusSampler([](){return InstallationStatus::InstallationFailed;});
                Serial.println(F("[main] HTTP_UPDATE_NO_UPDATES"));
                break;
            case HTTP_UPDATE_OK:
                fwService->setInstallationStatusSampler([](){return InstallationStatus::Installed;});
                Serial.println(F("[main] HTTP_UPDATE_OK"));
                ESP.restart();
                break;
        }

        return true;
    });

    fwService->setInstallationStatusSampler([fwService = fwService] () {
        return InstallationStatus::NotInstalled;
    });

    return fwService;
}

#endif //defined(ESP8266)
#endif //!defined(AO_CUSTOM_UPDATER) && !defined(AO_CUSTOM_WEBSOCKET)
