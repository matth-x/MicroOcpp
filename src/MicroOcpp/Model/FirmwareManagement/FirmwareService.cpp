// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/FirmwareManagement/FirmwareService.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Availability/AvailabilityService.h>
#include <MicroOcpp/Model/Boot/BootService.h>
#include <MicroOcpp/Model/Configuration/ConfigurationService.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlService.h>
#include <MicroOcpp/Model/Transactions/TransactionService16.h>
#include <MicroOcpp/Core/Request.h>

#include <MicroOcpp/Operations/UpdateFirmware.h>
#include <MicroOcpp/Operations/FirmwareStatusNotification.h>

#include <MicroOcpp/Platform.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16 && MO_ENABLE_FIRMWAREMANAGEMENT

//debug option: update immediately and don't wait for the retreive date
#ifndef MO_IGNORE_FW_RETR_DATE
#define MO_IGNORE_FW_RETR_DATE 0
#endif

namespace MicroOcpp {
namespace v16 {

#if MO_USE_FW_UPDATER != MO_FW_UPDATER_CUSTOM
bool setupDefaultFwUpdater(FirmwareService *fwService);
#endif

FirmwareService::FirmwareService(Context& context) : MemoryManaged("v16.Firmware.FirmwareService"), context(context), clock(context.getClock()) {

}

FirmwareService::~FirmwareService() {
    MO_FREE(buildNumber);
    buildNumber = nullptr;
    MO_FREE(location);
    location = nullptr;
}

bool FirmwareService::setup() {

    auto configService = context.getModel16().getConfigurationService();
    if (!configService) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    auto previousBuildNumberString = configService->declareConfiguration<const char*>("BUILD_NUMBER", "", MO_KEYVALUE_FN, Mutability::None);
    if (!previousBuildNumberString) {
        MO_DBG_ERR("declareConfiguration");
        return false;
    }

    auto bootSvc = context.getModel16().getBootService();
    if (!bootSvc) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    MO_BootNotificationData bnData = bootSvc->getBootNotificationData();

    const char *fwIdentifier = buildNumber ? buildNumber : bnData.firmwareVersion;
    if (fwIdentifier && strcmp(fwIdentifier, previousBuildNumberString->getString())) {

        MO_DBG_INFO("Firmware updated. Previous build number: %s, new build number: %s", previousBuildNumberString->getString(), fwIdentifier);
        notifyFirmwareUpdate = true; //will send this during `loop()`
    }
    MO_FREE(buildNumber);
    buildNumber = nullptr;

    context.getMessageService().registerOperation("UpdateFirmware", [] (Context& context) -> Operation* {
        return new v16::UpdateFirmware(context, *context.getModel16().getFirmwareService());});

    #if MO_ENABLE_MOCK_SERVER
    context.getMessageService().registerOperation("FirmwareStatusNotification", nullptr, nullptr, nullptr);
    #endif //MO_ENABLE_MOCK_SERVER

    auto rcService = context.getModel16().getRemoteControlService();
    if (!rcService) {
        MO_DBG_ERR("initialization error");
        return false;
    }

    rcService->addTriggerMessageHandler("FirmwareStatusNotification", [] (Context& context) -> Operation* {
        return new v16::FirmwareStatusNotification(context.getModel16().getFirmwareService()->getFirmwareStatus());});

#if MO_USE_FW_UPDATER != MO_FW_UPDATER_CUSTOM
    if (!setupDefaultFwUpdater(this)) {
        return false;
    }
#endif //MO_CUSTOM_UPDATER

    return true;
}

bool FirmwareService::setBuildNumber(const char *buildNumber) {
    if (!buildNumber) {
        MO_DBG_ERR("invalid arg");
        return false;
    }
    MO_FREE(this->buildNumber);
    this->buildNumber = nullptr;
    size_t size = strlen(buildNumber) + 1;
    this->buildNumber = static_cast<char*>(MO_MALLOC(getMemoryTag(), size));
    if (!this->buildNumber) {
        MO_DBG_ERR("OOM");
        return false;
    }
    auto ret = snprintf(this->buildNumber, size, "%s", buildNumber);
    if (ret < 0 || (size_t)ret >= size) {
        MO_DBG_ERR("snprintf: %i", ret);
        MO_FREE(this->buildNumber);
        this->buildNumber = nullptr;
        return false;
    }

    return true; //CS will be notified
}

void FirmwareService::loop() {

    if (ftpDownload && ftpDownload->isActive()) {
        ftpDownload->loop();
    }

    if (ftpDownload) {
        if (ftpDownload->isActive()) {
            ftpDownload->loop();
        } else {
            MO_DBG_DEBUG("Deinit FTP download");
            ftpDownload.reset();
        }
    }

    auto notification = getFirmwareStatusNotification();
    if (notification) {
        context.getMessageService().sendRequest(std::move(notification));
    }

    int32_t dtTransition;
    if (!clock.delta(clock.getUptime(), timestampTransition, dtTransition)) {
        dtTransition = -1;
    }

    if (dtTransition < delayTransition) {
        return;
    }

    int32_t dtRetreiveDate;
    if (!clock.delta(clock.now(), retreiveDate, dtRetreiveDate)) {
        dtRetreiveDate = 0;
    }

    if (retries > 0 && dtRetreiveDate >= 0) {

        if (stage == UpdateStage::Idle) {
            MO_DBG_INFO("Start update");

            auto availSvc = context.getModel16().getAvailabilityService();
            auto availSvcCp = availSvc ? availSvc->getEvse(0) : nullptr;
            if (availSvcCp) {
                availSvcCp->setAvailabilityVolatile(false);
            }
            if (onDownload == nullptr) {
                stage = UpdateStage::AfterDownload;
            } else {
                downloadIssued = true;
                stage = UpdateStage::AwaitDownload;
                timestampTransition = clock.getUptime();
                delayTransition = 2; //delay between state "Downloading" and actually starting the download
                return;
            }
        }

        if (stage == UpdateStage::AwaitDownload) {
            MO_DBG_INFO("Start download");
            stage = UpdateStage::Downloading;
            if (onDownload != nullptr) {
                onDownload(location ? location : "");
                timestampTransition = clock.getUptime();
                delayTransition = downloadStatusInput ? 1 : 30; //give the download at least 30s
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
                    MO_DBG_INFO("Download timeout or failed");
                    retreiveDate = clock.now();
                    clock.add(retreiveDate, retryInterval);
                    retries--;
                    resetStage();

                    timestampTransition = clock.getUptime();
                    delayTransition = 10;
                }
                return;
            } else {
                //if client doesn't report download state, assume download to be finished (at least 30s download time have passed until here)
                stage = UpdateStage::AfterDownload;
            }
        }

        if (stage == UpdateStage::AfterDownload) {
            bool ongoingTx = false;
            for (unsigned int evseId = 0; evseId < context.getModel16().getNumEvseId(); evseId++) {
                auto txSvc = context.getModel16().getTransactionService();
                auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
                if (txSvcEvse && txSvcEvse->getTransaction() && txSvcEvse->getTransaction()->isRunning()) {
                    ongoingTx = true;
                    break;
                }
            }

            if (!ongoingTx) {
                if (onInstall == nullptr) {
                    stage = UpdateStage::Installing;
                } else {
                    stage = UpdateStage::AwaitInstallation;
                }
                timestampTransition = clock.getUptime();
                delayTransition = 2;
                installationIssued = true;
            }

            return;
        }

        if (stage == UpdateStage::AwaitInstallation) {
            MO_DBG_INFO("Installing");
            stage = UpdateStage::Installing;

            if (onInstall) {
                onInstall(location ? location : ""); //may restart the device on success

                timestampTransition = clock.getUptime();
                delayTransition = installationStatusInput ? 1 : 120;
            }
            return;
        }

        if (stage == UpdateStage::Installing) {

            if (installationStatusInput) {
                if (installationStatusInput() == InstallationStatus::Installed) {
                    MO_DBG_INFO("FW update finished");
                    //Charger may reboot during onInstall. If it doesn't, server will send Reset request
                    resetStage();
                    retries = 0; //End of update routine
                    stage = UpdateStage::Installed;
                    MO_FREE(location);
                    location = nullptr;
                } else if (installationStatusInput() == InstallationStatus::InstallationFailed) {
                    MO_DBG_INFO("Installation timeout or failed! Retry");
                    retreiveDate = clock.now();
                    clock.add(retreiveDate, retryInterval);
                    retries--;
                    resetStage();

                    timestampTransition = clock.getUptime();
                    delayTransition = 10;
                }
                return;
            } else {
                MO_DBG_INFO("FW update finished");
                //Charger may reboot during onInstall. If it doesn't, server will send Reset request
                resetStage();
                stage = UpdateStage::Installed;
                retries = 0; //End of update routine
                MO_FREE(location);
                location = nullptr;
                return;
            }
        }

        //should never reach this code
        MO_DBG_ERR("Firmware update failed");
        retries = 0;
        resetStage();
        stage = UpdateStage::InternalError;
        MO_FREE(location);
        location = nullptr;
    }
}

bool FirmwareService::scheduleFirmwareUpdate(const char *location, Timestamp retreiveDate, unsigned int retries, unsigned int retryInterval) {

    if (!location) {
        MO_DBG_ERR("invalid arg");
        return false;
    }

    if (!onDownload && !onInstall) {
        MO_DBG_ERR("FW service not configured");
        stage = UpdateStage::InternalError; //will send "InstallationFailed" and not proceed with update
        return false;
    }

    MO_FREE(this->location);
    this->location = nullptr;
    size_t len = strlen(location);
    this->location = static_cast<char*>(MO_MALLOC(getMemoryTag(), len));
    if (!this->location) {
        MO_DBG_ERR("OOM");
        return false;
    }
    auto ret = snprintf(this->location, len, "%s", location);
    if (ret < 0 || (size_t)ret >= len) {
        MO_DBG_ERR("snprintf: %i", ret);
        MO_FREE(this->location);
        this->location = nullptr;
        return false;
    }

    this->retreiveDate = retreiveDate;
    this->retries = retries;
    this->retryInterval = retryInterval;

    if (MO_IGNORE_FW_RETR_DATE) {
        MO_DBG_DEBUG("ignore FW update retreive date");
        this->retreiveDate = clock.now();
    }

    char dbuf [MO_JSONDATE_SIZE] = {'\0'};
    if (!clock.toJsonString(this->retreiveDate, dbuf, sizeof(dbuf))) {
        MO_DBG_ERR("internal error");
        dbuf[0] = '\0';
    }

    MO_DBG_INFO("Scheduled FW update!\n" \
                    "                  location = %s\n" \
                    "                  retrieveDate = %s\n" \
                    "                  retries = %u" \
                    ", retryInterval = %u",
            location ? location : "",
            dbuf,
            this->retries,
            this->retryInterval);

    timestampTransition = clock.getUptime();
    delayTransition = 1;

    resetStage();

    return true;
}

FirmwareStatus FirmwareService::getFirmwareStatus() {

    if (stage == UpdateStage::Installed) {
        return FirmwareStatus::Installed;
    } else if (stage == UpdateStage::InternalError) {
        return FirmwareStatus::InstallationFailed; 
    }

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
    if (notifyFirmwareUpdate) {
        notifyFirmwareUpdate = false;

        lastReportedStatus = FirmwareStatus::Installed;
        auto fwNotificationMsg = new v16::FirmwareStatusNotification(lastReportedStatus);
        auto fwNotification = makeRequest(context, fwNotificationMsg);
        return fwNotification;
    }

    if (getFirmwareStatus() != lastReportedStatus) {
        lastReportedStatus = getFirmwareStatus();
        if (lastReportedStatus != FirmwareStatus::Idle) {
            auto fwNotificationMsg = new v16::FirmwareStatusNotification(lastReportedStatus);
            auto fwNotification = makeRequest(context, fwNotificationMsg);
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

void FirmwareService::setDownloadFileWriter(std::function<size_t(const unsigned char *buf, size_t size)> firmwareWriter, std::function<void(MO_FtpCloseReason)> onClose) {

    this->onDownload = [this, firmwareWriter, onClose] (const char *location) -> bool {

        auto ftpClient = context.getFtpClient();
        if (!ftpClient) {
            MO_DBG_ERR("FTP client not set");
            this->ftpDownloadStatus = DownloadStatus::DownloadFailed;
            return false;
        }

        this->ftpDownload = ftpClient->getFile(location, firmwareWriter,
            [this, onClose] (MO_FtpCloseReason reason) -> void {
                if (reason == MO_FtpCloseReason_Success) {
                    MO_DBG_INFO("FTP download success");
                    this->ftpDownloadStatus = DownloadStatus::Downloaded;
                } else {
                    MO_DBG_INFO("FTP download failure (%i)", reason);
                    this->ftpDownloadStatus = DownloadStatus::DownloadFailed;
                }

                onClose(reason);
            });

        if (this->ftpDownload) {
            this->ftpDownloadStatus = DownloadStatus::NotDownloaded;
            return true;
        } else {
            this->ftpDownloadStatus = DownloadStatus::DownloadFailed;
            return false;
        }
    };

    this->downloadStatusInput = [this] () {
        return this->ftpDownloadStatus;
    };
}

void FirmwareService::setFtpServerCert(const char *cert) {
    this->ftpServerCert = cert;
}

} //namespace v16
} //namespace MicroOcpp

#if MO_USE_FW_UPDATER == MO_FW_UPDATER_BUILTIN_ESP32

#include <Update.h>

bool MicroOcpp::v16::setupDefaultFwUpdater(MicroOcpp::v16::FirmwareService *fwService) {
    fwService->setDownloadFileWriter(
        [fwService] (const unsigned char *data, size_t size) -> size_t {
            if (!Update.isRunning()) {
                MO_DBG_DEBUG("start writing FW");
                MO_DBG_WARN("Built-in updater for ESP32 is only intended for demonstration purposes");
                fwService->setInstallationStatusInput([](){return InstallationStatus::NotInstalled;});

                auto ret = Update.begin();
                if (!ret) {
                    MO_DBG_ERR("cannot start update: %i", ret);
                    return 0;
                }
            }

            size_t written = Update.write((uint8_t*) data, size);

            #if MO_DBG_LEVEL >= MO_DL_INFO
            {
                size_t progress = Update.progress();

                bool printProgress = false;

                if (progress <= 10000) {
                    size_t p1k = progress / 1000;
                    printProgress = progress < p1k * 1000 + written && progress >= p1k * 1000;
                } else if (progress <= 100000) {
                    size_t p10k = progress / 10000;
                    printProgress = progress < p10k * 10000 + written && progress >= p10k * 10000;
                } else {
                    size_t p100k = progress / 100000;
                    printProgress = progress < p100k * 100000 + written && progress >= p100k * 100000;
                }

                if (printProgress) {
                    MO_DBG_INFO("update progress: %zu kB", progress / 1000);
                }
            }
            #endif //MO_DBG_LEVEL >= MO_DL_DEBUG

            return written;
        }, [] (MO_FtpCloseReason reason) {
            if (reason != MO_FtpCloseReason_Success) {
                Update.abort();
            }
        });

    fwService->setOnInstall([fwService] (const char *location) {

        if (Update.isRunning() && Update.end(true)) {
            MO_DBG_DEBUG("update success");
            fwService->setInstallationStatusInput([](){return InstallationStatus::Installed;});

            ESP.restart();
        } else {
            MO_DBG_ERR("update failed");
            fwService->setInstallationStatusInput([](){return InstallationStatus::InstallationFailed;});
        }

        return true;
    });

    fwService->setInstallationStatusInput([] () {
        return InstallationStatus::NotInstalled;
    });

    return true;
}

#elif MO_USE_FW_UPDATER == MO_FW_UPDATER_BUILTIN_ESP8266

#include <ESP8266httpUpdate.h>

bool MicroOcpp::v16::setupDefaultFwUpdater(MicroOcpp::v16::FirmwareService *fwService) {

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

    return true;
}

#endif //MO_USE_FW_UPDATER

#endif //MO_ENABLE_V16 && MO_ENABLE_FIRMWAREMANAGEMENT
