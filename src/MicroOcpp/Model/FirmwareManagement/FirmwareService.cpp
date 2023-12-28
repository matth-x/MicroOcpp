// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Model/FirmwareManagement/FirmwareService.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/ConnectorBase/Connector.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/OperationRegistry.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>

#include <MicroOcpp/Operations/UpdateFirmware.h>
#include <MicroOcpp/Operations/FirmwareStatusNotification.h>

#include <MicroOcpp/Platform.h>
#include <MicroOcpp/Debug.h>

//debug option: update immediately and don't wait for the retreive date
#ifndef MO_IGNORE_FW_RETR_DATE
#define MO_IGNORE_FW_RETR_DATE 0
#endif

using namespace MicroOcpp;
using MicroOcpp::Ocpp16::FirmwareStatus;

FirmwareService::FirmwareService(Context& context) : context(context) {
    
    context.getOperationRegistry().registerOperation("UpdateFirmware", [this] () {
        return new Ocpp16::UpdateFirmware(*this);});

    //Register message handler for TriggerMessage operation
    context.getOperationRegistry().registerOperation("FirmwareStatusNotification", [this] () {
        return new Ocpp16::FirmwareStatusNotification(getFirmwareStatus());});
}

void FirmwareService::setBuildNumber(const char *buildNumber) {
    if (buildNumber == nullptr)
        return;
    this->buildNumber = buildNumber;
    previousBuildNumberString = declareConfiguration<const char*>("BUILD_NUMBER", this->buildNumber.c_str(), MO_KEYVALUE_FN, false, false, false);
    checkedSuccessfulFwUpdate = false; //--> CS will be notified
}

void FirmwareService::loop() {
    auto notification = getFirmwareStatusNotification();
    if (notification) {
        context.initiateRequest(std::move(notification));
    }

    if (mocpp_tick_ms() - timestampTransition < delayTransition) {
        return;
    }

    auto& timestampNow = context.getModel().getClock().now();
    if (retries > 0 && timestampNow >= retreiveDate) {

        if (stage == UpdateStage::Idle) {
            MO_DBG_INFO("Start update");

            if (context.getModel().getNumConnectors() > 0) {
                auto cp = context.getModel().getConnector(0);
                cp->setAvailabilityVolatile(false);
            }
            if (onDownload == nullptr) {
                stage = UpdateStage::AfterDownload;
            } else {
                downloadIssued = true;
                stage = UpdateStage::AwaitDownload;
                timestampTransition = mocpp_tick_ms();
                delayTransition = 5000; //delay between state "Downloading" and actually starting the download
                return;
            }
        }

        if (stage == UpdateStage::AwaitDownload) {
            MO_DBG_INFO("Start download");
            stage = UpdateStage::Downloading;
            if (onDownload != nullptr) {
                onDownload(location.c_str());
                timestampTransition = mocpp_tick_ms();
                delayTransition = downloadStatusInput ? 1000 : 30000; //give the download at least 30s
                return;
            }
        }

        if (stage == UpdateStage::Downloading) {

            if (downloadStatusInput) {
                //check if client reports download to be finished

                if (downloadStatusInput() == DownloadStatus::Downloaded) {
                    //passed download stage
                    stage = UpdateStage::AfterDownload;
                } else if (downloadStatusInput() == DownloadStatus::DownloadFailed) {
                    MO_DBG_INFO("Download timeout or failed! Retry");
                    retreiveDate = timestampNow;
                    retreiveDate += retryInterval;
                    retries--;
                    resetStage();

                    timestampTransition = mocpp_tick_ms();
                    delayTransition = 10000;
                }
                return;
            } else {
                //if client doesn't report download state, assume download to be finished (at least 30s download time have passed until here)
                if (downloadStatusInput == nullptr) {
                    stage = UpdateStage::AfterDownload;
                }
            }
        }

        if (stage == UpdateStage::AfterDownload) {
            bool ongoingTx = false;
            for (unsigned int cId = 0; cId < context.getModel().getNumConnectors(); cId++) {
                auto connector = context.getModel().getConnector(cId);
                if (connector && connector->getTransaction() && connector->getTransaction()->isRunning()) {
                    ongoingTx = true;
                    break;
                }
            }

            if (!ongoingTx) {
                stage = UpdateStage::AwaitInstallation;
                installationIssued = true;

                timestampTransition = mocpp_tick_ms();
                delayTransition = 10000;
            }

            return;
        }

        if (stage == UpdateStage::AwaitInstallation) {
            MO_DBG_INFO("Installing");
            stage = UpdateStage::Installing;

            if (onInstall) {
                onInstall(location.c_str()); //should restart the device on success
            } else {
                MO_DBG_WARN("onInstall must be set! (see setOnInstall). Will abort");
            }

            timestampTransition = mocpp_tick_ms();
            delayTransition = installationStatusInput ? 1000 : 120 * 1000;
            return;
        }

        if (stage == UpdateStage::Installing) {

            if (installationStatusInput) {
                if (installationStatusInput() == InstallationStatus::Installed) {
                    MO_DBG_INFO("FW update finished");
                    //Client should reboot during onInstall. If not, client is responsible to reboot at a later point
                    resetStage();
                    retries = 0; //End of update routine. Client must reboot on its own
                } else if (installationStatusInput() == InstallationStatus::InstallationFailed) {
                    MO_DBG_INFO("Installation timeout or failed! Retry");
                    retreiveDate = timestampNow;
                    retreiveDate += retryInterval;
                    retries--;
                    resetStage();

                    timestampTransition = mocpp_tick_ms();
                    delayTransition = 10000;
                }
                return;
            } else {
                MO_DBG_INFO("FW update finished");
                //Client should reboot during onInstall. If not, client is responsible to reboot at a later point
                resetStage();
                retries = 0; //End of update routine. Client must reboot on its own
                return;
            }
        }

        //should never reach this code
        MO_DBG_ERR("Firmware update failed");
        retries = 0;
        resetStage();
    }
}

void FirmwareService::scheduleFirmwareUpdate(const char *location, Timestamp retreiveDate, unsigned int retries, unsigned int retryInterval) {
    this->location = location;
    this->retreiveDate = retreiveDate;
    this->retries = retries;
    this->retryInterval = retryInterval;

    if (MO_IGNORE_FW_RETR_DATE) {
        MO_DBG_DEBUG("ignore FW update retreive date");
        this->retreiveDate = context.getModel().getClock().now();
    }

    char dbuf [JSONDATE_LENGTH + 1] = {'\0'};
    this->retreiveDate.toJsonString(dbuf, JSONDATE_LENGTH + 1);

#if MO_DBG_LEVEL >= MO_DL_INFO
    MO_DBG_INFO("Scheduled FW update!\n" \
                    "                  location = %s\n" \
                    "                  retrieveDate = %s\n" \
                    "                  retries = %u" \
                    ", retryInterval = %u",
            this->location.c_str(),
            dbuf,
            this->retries,
            this->retryInterval);
#endif

    timestampTransition = mocpp_tick_ms();
    delayTransition = 1000;

    resetStage();
}

FirmwareStatus FirmwareService::getFirmwareStatus() {
    if (installationIssued) {
        if (installationStatusInput != nullptr) {
            if (installationStatusInput() == InstallationStatus::Installed) {
                return FirmwareStatus::Installed;
            } else if (installationStatusInput() == InstallationStatus::InstallationFailed) {
                return FirmwareStatus::InstallationFailed;
            }
        }
        if (onInstall != nullptr)
            return FirmwareStatus::Installing;
    }
    
    if (downloadIssued) {
        if (downloadStatusInput != nullptr) {
            if (downloadStatusInput() == DownloadStatus::Downloaded) {
                return FirmwareStatus::Downloaded;
            } else if (downloadStatusInput() == DownloadStatus::DownloadFailed) {
                return FirmwareStatus::DownloadFailed;
            }
        }
        if (onDownload != nullptr)
            return FirmwareStatus::Downloading;
    }

    return FirmwareStatus::Idle;
}

std::unique_ptr<Request> FirmwareService::getFirmwareStatusNotification() {
    /*
     * Check if FW has been updated previously, but only once
     */
    if (!checkedSuccessfulFwUpdate && !buildNumber.empty() && previousBuildNumberString != nullptr) {
        checkedSuccessfulFwUpdate = true;

        MO_DBG_DEBUG("Previous build number: %s, new build number: %s", previousBuildNumberString->getString(), buildNumber.c_str());
        
        if (buildNumber.compare(previousBuildNumberString->getString())) {
            //new FW
            previousBuildNumberString->setString(buildNumber.c_str());
            configuration_save();

            buildNumber.clear();

            lastReportedStatus = FirmwareStatus::Installed;
            auto fwNotificationMsg = new Ocpp16::FirmwareStatusNotification(lastReportedStatus);
            auto fwNotification = makeRequest(fwNotificationMsg);
            return fwNotification;
        }
    }

    if (getFirmwareStatus() != lastReportedStatus) {
        lastReportedStatus = getFirmwareStatus();
        if (lastReportedStatus != FirmwareStatus::Idle && lastReportedStatus != FirmwareStatus::Installed) {
            auto fwNotificationMsg = new Ocpp16::FirmwareStatusNotification(lastReportedStatus);
            auto fwNotification = makeRequest(fwNotificationMsg);
            return fwNotification;
        }
    }

    return nullptr;
}

void FirmwareService::setOnDownload(std::function<bool(const char *location)> onDownload) {
    this->onDownload = onDownload;
}

void FirmwareService::setDownloadStatusInput(std::function<DownloadStatus()> downloadStatusInput) {
    this->downloadStatusInput = downloadStatusInput;
}

void FirmwareService::setOnInstall(std::function<bool(const char *location)> onInstall) {
    this->onInstall = onInstall;
}

void FirmwareService::setInstallationStatusInput(std::function<InstallationStatus()> installationStatusInput) {
    this->installationStatusInput = installationStatusInput;
}

void FirmwareService::resetStage() {
    stage = UpdateStage::Idle;
    downloadIssued = false;
    installationIssued = false;
}

#if !defined(MO_CUSTOM_UPDATER) && !defined(MO_CUSTOM_WS)
#if defined(ESP32)

#include <HTTPUpdate.h>

FirmwareService *EspWiFi::makeFirmwareService(Context& context) {
    FirmwareService *fwService = new FirmwareService(context);

    /*
     * example of how to integrate a separate download phase (optional)
     */
#if 0 //separate download phase
    fwService->setOnDownload([] (const char *location) {
        //download the new binary
        //...
        return true;
    });

    fwService->setDownloadStatusInput([] () {
        //report the download progress
        //...
        return DownloadStatus::NotDownloaded;
    });
#endif //separate download phase

    fwService->setOnInstall([fwService] (const char *location) {

        MO_DBG_WARN("Built-in updater for ESP32 is only intended for demonstration purposes. HTTP support only");

        fwService->setInstallationStatusInput([](){return InstallationStatus::NotInstalled;});

        WiFiClient client;
        //WiFiClientSecure client;
        //client.setCACert(rootCACertificate);
        client.setTimeout(60); //in seconds
        
        // httpUpdate.setLedPin(LED_BUILTIN, HIGH);
        t_httpUpdate_return ret = httpUpdate.update(client, location);

        switch (ret) {
            case HTTP_UPDATE_FAILED:
                fwService->setInstallationStatusInput([](){return InstallationStatus::InstallationFailed;});
                MO_DBG_WARN("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
                break;
            case HTTP_UPDATE_NO_UPDATES:
                fwService->setInstallationStatusInput([](){return InstallationStatus::InstallationFailed;});
                MO_DBG_WARN("HTTP_UPDATE_NO_UPDATES");
                break;
            case HTTP_UPDATE_OK:
                fwService->setInstallationStatusInput([](){return InstallationStatus::Installed;});
                MO_DBG_INFO("HTTP_UPDATE_OK");
                ESP.restart();
                break;
        }

        return true;
    });

    fwService->setInstallationStatusInput([] () {
        return InstallationStatus::NotInstalled;
    });

    return fwService;
}

#elif defined(ESP8266)

#include <ESP8266httpUpdate.h>

FirmwareService *EspWiFi::makeFirmwareService(Context& context) {
    FirmwareService *fwService = new FirmwareService(context);

    fwService->setOnInstall([fwService] (const char *location) {
        
        MO_DBG_WARN("Built-in updater for ESP8266 is only intended for demonstration purposes. HTTP support only");

        WiFiClient client;
        //WiFiClientSecure client;
        //client.setCACert(rootCACertificate);
        client.setTimeout(60); //in seconds

        //ESPhttpUpdate.setLedPin(downloadStatusLedPin);

        HTTPUpdateResult ret = ESPhttpUpdate.update(client, location);

        switch (ret) {
            case HTTP_UPDATE_FAILED:
                fwService->setInstallationStatusInput([](){return InstallationStatus::InstallationFailed;});
                MO_DBG_WARN("HTTP_UPDATE_FAILED Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                break;
            case HTTP_UPDATE_NO_UPDATES:
                fwService->setInstallationStatusInput([](){return InstallationStatus::InstallationFailed;});
                MO_DBG_WARN("HTTP_UPDATE_NO_UPDATES");
                break;
            case HTTP_UPDATE_OK:
                fwService->setInstallationStatusInput([](){return InstallationStatus::Installed;});
                MO_DBG_INFO("HTTP_UPDATE_OK");
                ESP.restart();
                break;
        }

        return true;
    });

    fwService->setInstallationStatusInput([] () {
        return InstallationStatus::NotInstalled;
    });

    return fwService;
}

#endif //defined(ESP8266)
#endif //!defined(MO_CUSTOM_UPDATER) && !defined(MO_CUSTOM_WS)
