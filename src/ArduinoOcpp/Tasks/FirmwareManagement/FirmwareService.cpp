// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Tasks/FirmwareManagement/FirmwareService.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>

#include <ArduinoOcpp/MessagesV16/FirmwareStatusNotification.h>

#include <ArduinoOcpp/Platform.h>
#include <ArduinoOcpp/Debug.h>

using namespace ArduinoOcpp;
using ArduinoOcpp::Ocpp16::FirmwareStatus;

void FirmwareService::setBuildNumber(const char *buildNumber) {
    if (buildNumber == nullptr)
        return;
    this->buildNumber = buildNumber;
    previousBuildNumber = declareConfiguration<const char*>("BUILD_NUMBER", buildNumber, CONFIGURATION_FN, false, false, true, false);
    checkedSuccessfulFwUpdate = false; //--> CS will be notified
}

void FirmwareService::loop() {
    auto notification = getFirmwareStatusNotification();
    if (notification) {
        context.initiateOperation(std::move(notification));
    }

    if (ao_tick_ms() - timestampTransition < delayTransition) {
        return;
    }

    OcppTimestamp timestampNow = context.getOcppModel().getOcppTime().getOcppTimestampNow();
    if (retries > 0 && timestampNow >= retreiveDate) {

        auto cpStatusService = context.getOcppModel().getChargePointStatusService();
        
        //if (!downloadIssued) {
        if (stage == UpdateStage::Idle) {
            AO_DBG_INFO("Start update");

            if (cpStatusService) {
                ConnectorStatus *evse = cpStatusService->getConnector(0);
                if (!availabilityRestore) {
                    availabilityRestore = (evse->getAvailability() == AVAILABILITY_OPERATIVE);
                }
                evse->setAvailability(false);
            }
            if (onDownload == nullptr) {
                stage = UpdateStage::AfterDownload;
            } else {
                downloadIssued = true;
                stage = UpdateStage::AwaitDownload;
                timestampTransition = ao_tick_ms();
                delayTransition = 5000; //delay between state "Downloading" and actually starting the download
                return;
            }
        }

        //if (!onDownloadCalled) {
        if (stage == UpdateStage::AwaitDownload) {
            AO_DBG_INFO("Start download");
            stage = UpdateStage::Downloading;
            if (onDownload != nullptr) {
                onDownload(location);
                timestampTransition = ao_tick_ms();
                delayTransition = 30000; //give the download at least 30s
                return;
            }
        }

        const int DOWNLOAD_TIMEOUT = 120;
        //if (downloadIssued && !installationIssued &&
        if (stage == UpdateStage::Downloading) {

            //check if client reports download to be finished
            if (downloadStatusSampler != nullptr && downloadStatusSampler() == DownloadStatus::Downloaded) {
                stage = UpdateStage::AfterDownload;
            }

            //if client doesn't report download state, assume download to be finished (at least 30s download time have passed until here)
            if (downloadStatusSampler == nullptr) {
                stage = UpdateStage::AfterDownload;
            }

            //check for timeout or error condition
            if (timestampNow - retreiveDate >= DOWNLOAD_TIMEOUT
                    || (downloadStatusSampler != nullptr && downloadStatusSampler() == DownloadStatus::DownloadFailed)) {
                
                AO_DBG_INFO("Download timeout or failed! Retry");
                if (retryInterval < DOWNLOAD_TIMEOUT)
                    retreiveDate = timestampNow;
                else
                    retreiveDate += retryInterval;
                retries--;
                resetStage();

                timestampTransition = ao_tick_ms();
                delayTransition = 10000;
                return;
            }

        }

        //if (!installationIssued) {
        if (stage == UpdateStage::AfterDownload) {
            bool ongoingTx = false;
            if (cpStatusService) {
                for (int i = 0; i < cpStatusService->getNumConnectors(); i++) {
                    auto connector = cpStatusService->getConnector(i);
                    if (connector && connector->getTransactionId() >= 0) {
                        ongoingTx = true;
                        break;
                    }
                }
            }

            if (!ongoingTx) {
                stage = UpdateStage::AwaitInstallation;
                installationIssued = true;

                timestampTransition = ao_tick_ms();
                delayTransition = 10000;
            }

            return;
        }

        if (stage == UpdateStage::AwaitInstallation) {
            AO_DBG_INFO("Installing");
            stage = UpdateStage::Installing;

            if (onInstall != nullptr) {
                onInstall(location); //should restart the device on success
            } else {
                AO_DBG_WARN("onInstall must be set! (see setOnInstall). Will abort");
            }

            timestampTransition = ao_tick_ms();
            delayTransition = 40000;
            return;
        }

        if (stage == UpdateStage::Installing) {

            //check if client reports installation to be finished
            if ((installationStatusSampler != nullptr && installationStatusSampler() == InstallationStatus::Installed)
                    //if client doesn't report installation state, assume download to be finished (at least 40s installation time have passed until here)
                      || (installationStatusSampler == nullptr)) {
                //Client should reboot during onInstall. If not, client is responsible to reboot at a later point
                resetStage();
                retries = 0; //End of update routine. Client must reboot on its own
                if (availabilityRestore) {
                    if (cpStatusService && cpStatusService->getConnector(0)) {
                        cpStatusService->getConnector(0)->setAvailability(true);
                    }
                }
                return;
            }

            //check for timeout or error condition
            const int INSTALLATION_TIMEOUT = 120;
            if ((timestampNow - retreiveDate >= INSTALLATION_TIMEOUT + DOWNLOAD_TIMEOUT)
                    || (installationStatusSampler != nullptr && installationStatusSampler() == InstallationStatus::InstallationFailed)) {

                AO_DBG_INFO("Installation timeout or failed! Retry");
                if (retryInterval < INSTALLATION_TIMEOUT + DOWNLOAD_TIMEOUT)
                    retreiveDate = timestampNow;
                else
                    retreiveDate += retryInterval;
                retries--;
                resetStage();

                timestampTransition = ao_tick_ms();
                delayTransition = 10000;
                return;
            }
        }

        //should never reach this code
        AO_DBG_ERR("Firmware update failed");
        retries = 0;
        resetStage();
    }
}

void FirmwareService::scheduleFirmwareUpdate(String &location, OcppTimestamp retreiveDate, int retries, unsigned int retryInterval) {
    this->location = String(location);
    this->retreiveDate = retreiveDate;
    this->retries = retries;
    this->retryInterval = retryInterval;

    char dbuf [JSONDATE_LENGTH + 1] = {'\0'};
    this->retreiveDate.toJsonString(dbuf, JSONDATE_LENGTH + 1);

    AO_DBG_INFO("Scheduled FW update!\n" \
                    "                  location = %s\n" \
                    "                  retrieveDate = %s\n" \
                    "                  retries = %i" \
                    ", retryInterval = %u",
            this->location.c_str(),
            dbuf,
            this->retries,
            this->retryInterval);

    resetStage();
}

FirmwareStatus FirmwareService::getFirmwareStatus() {
    if (installationIssued) {
        if (installationStatusSampler != nullptr) {
            if (installationStatusSampler() == InstallationStatus::Installed) {
                return FirmwareStatus::Installed;
            } else if (installationStatusSampler() == InstallationStatus::InstallationFailed) {
                return FirmwareStatus::InstallationFailed;
            }
        }
        if (onInstall != nullptr)
            return FirmwareStatus::Installing;
    }
    
    if (downloadIssued) {
        if (downloadStatusSampler != nullptr) {
            if (downloadStatusSampler() == DownloadStatus::Downloaded) {
                return FirmwareStatus::Downloaded;
            } else if (downloadStatusSampler() == DownloadStatus::DownloadFailed) {
                return FirmwareStatus::DownloadFailed;
            }
        }
        if (onDownload != nullptr)
            return FirmwareStatus::Downloading;
    }

    return FirmwareStatus::Idle;
}

std::unique_ptr<OcppOperation> FirmwareService::getFirmwareStatusNotification() {
    /*
     * Check if FW has been updated previously, but only once
     */
    if (!checkedSuccessfulFwUpdate && buildNumber != nullptr && previousBuildNumber != nullptr) {
        checkedSuccessfulFwUpdate = true;
        
        size_t buildNoSize = previousBuildNumber->getBuffsize();
        if (strncmp(buildNumber, *previousBuildNumber, buildNoSize)) {
            //new FW
            previousBuildNumber->setValue(buildNumber, strlen(buildNumber) + 1);
            configuration_save();

            lastReportedStatus = FirmwareStatus::Installed;
            OcppMessage *fwNotificationMsg = new Ocpp16::FirmwareStatusNotification(lastReportedStatus);
            auto fwNotification = makeOcppOperation(fwNotificationMsg);
            return fwNotification;
        }
    }

    if (getFirmwareStatus() != lastReportedStatus) {
        lastReportedStatus = getFirmwareStatus();
        if (lastReportedStatus != FirmwareStatus::Idle && lastReportedStatus != FirmwareStatus::Installed) {
            OcppMessage *fwNotificationMsg = new Ocpp16::FirmwareStatusNotification(lastReportedStatus);
            auto fwNotification = makeOcppOperation(fwNotificationMsg);
            return fwNotification;
        }
    }

    return nullptr;
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

void FirmwareService::resetStage() {
    stage = UpdateStage::Idle;
    downloadIssued = false;
    installationIssued = false;
}

#if !defined(AO_CUSTOM_UPDATER) && !defined(AO_CUSTOM_WS)
#if defined(ESP32)

#include <HTTPUpdate.h>

FirmwareService *EspWiFi::makeFirmwareService(OcppEngine& context, const char *buildNumber) {
    FirmwareService *fwService = new FirmwareService(context);
    fwService->setBuildNumber(buildNumber);

    /*
     * example of how to integrate a separate download phase (optional)
     */
#if 0 //separate download phase
    fwService->setOnDownload([] (String &location) {
        //download the new binary
        //...
        return true;
    });

    fwService->setDownloadStatusSampler([] () {
        //report the download progress
        //...
        return DownloadStatus::NotDownloaded;
    });
#endif //separate download phase

    fwService->setOnInstall([fwService] (String &location) {

        fwService->setInstallationStatusSampler([](){return InstallationStatus::NotInstalled;});

        WiFiClient client;
        //WiFiClientSecure client;
        //client.setCACert(rootCACertificate);
        client.setTimeout(60); //in seconds
        
        // httpUpdate.setLedPin(LED_BUILTIN, HIGH);
        t_httpUpdate_return ret = httpUpdate.update(client, location.c_str());

        switch (ret) {
            case HTTP_UPDATE_FAILED:
                fwService->setInstallationStatusSampler([](){return InstallationStatus::InstallationFailed;});
                AO_DBG_WARN("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
                break;
            case HTTP_UPDATE_NO_UPDATES:
                fwService->setInstallationStatusSampler([](){return InstallationStatus::InstallationFailed;});
                AO_DBG_WARN("HTTP_UPDATE_NO_UPDATES");
                break;
            case HTTP_UPDATE_OK:
                fwService->setInstallationStatusSampler([](){return InstallationStatus::Installed;});
                AO_DBG_INFO("HTTP_UPDATE_OK");
                ESP.restart();
                break;
        }

        return true;
    });

    fwService->setInstallationStatusSampler([] () {
        return InstallationStatus::NotInstalled;
    });

    return fwService;
}

#elif defined(ESP8266)

#include <ESP8266httpUpdate.h>

FirmwareService *EspWiFi::makeFirmwareService(OcppEngine& context, const char *buildNumber) {
    FirmwareService *fwService = new FirmwareService(context);
    fwService->setBuildNumber(buildNumber);

    fwService->setOnInstall([fwService] (String &location) {
        
        WiFiClient client;
        //WiFiClientSecure client;
        //client.setCACert(rootCACertificate);
        client.setTimeout(60); //in seconds

        //ESPhttpUpdate.setLedPin(downloadStatusLedPin);

        HTTPUpdateResult ret = ESPhttpUpdate.update(client, location.c_str());

        switch (ret) {
            case HTTP_UPDATE_FAILED:
                fwService->setInstallationStatusSampler([](){return InstallationStatus::InstallationFailed;});
                AO_DBG_WARN("HTTP_UPDATE_FAILED Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                break;
            case HTTP_UPDATE_NO_UPDATES:
                fwService->setInstallationStatusSampler([](){return InstallationStatus::InstallationFailed;});
                AO_DBG_WARN("HTTP_UPDATE_NO_UPDATES");
                break;
            case HTTP_UPDATE_OK:
                fwService->setInstallationStatusSampler([](){return InstallationStatus::Installed;});
                AO_DBG_INFO("HTTP_UPDATE_OK");
                ESP.restart();
                break;
        }

        return true;
    });

    fwService->setInstallationStatusSampler([] () {
        return InstallationStatus::NotInstalled;
    });

    return fwService;
}

#endif //defined(ESP8266)
#endif //!defined(AO_CUSTOM_UPDATER) && !defined(AO_CUSTOM_WS)
