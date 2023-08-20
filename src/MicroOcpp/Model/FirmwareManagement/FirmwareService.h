// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef FIRMWARESERVICE_H
#define FIRMWARESERVICE_H

#include <functional>
#include <memory>

#include <ArduinoOcpp/Core/ConfigurationKeyValue.h>
#include <ArduinoOcpp/Model/FirmwareManagement/FirmwareStatus.h>
#include <ArduinoOcpp/Core/Time.h>

namespace ArduinoOcpp {

enum class DownloadStatus {
    NotDownloaded, // == before download or during download
    Downloaded,
    DownloadFailed
};

enum class InstallationStatus {
    NotInstalled, // == before installation or during installation
    Installed,
    InstallationFailed
};

class Context;
class Request;

class FirmwareService {
private:
    Context& context;
    
    std::shared_ptr<Configuration<const char *>> previousBuildNumber;
    std::string buildNumber;

    std::function<DownloadStatus()> downloadStatusInput;
    bool downloadIssued = false;

    std::function<InstallationStatus()> installationStatusInput;
    bool installationIssued = false;

    Ocpp16::FirmwareStatus lastReportedStatus = Ocpp16::FirmwareStatus::Idle;
    bool checkedSuccessfulFwUpdate = false;

    std::string location {};
    Timestamp retreiveDate = Timestamp();
    int retries = 0;
    unsigned int retryInterval = 0;

    std::function<bool(const std::string &location)> onDownload;
    std::function<bool(const std::string &location)> onInstall;

    unsigned long delayTransition = 0;
    unsigned long timestampTransition = 0;

    enum class UpdateStage {
        Idle,
        AwaitDownload,
        Downloading,
        AfterDownload,
        AwaitInstallation,
        Installing
    } stage;

    void resetStage();

    std::unique_ptr<Request> getFirmwareStatusNotification();

public:
    FirmwareService(Context& context);

    void setBuildNumber(const char *buildNumber);

    void loop();

    void scheduleFirmwareUpdate(const std::string &location, Timestamp retreiveDate, int retries = 1, unsigned int retryInterval = 0);

    Ocpp16::FirmwareStatus getFirmwareStatus();

    void setOnDownload(std::function<bool(const std::string &location)> onDownload);

    void setDownloadStatusInput(std::function<DownloadStatus()> downloadStatusInput);

    void setOnInstall(std::function<bool(const std::string &location)> onInstall);

    void setInstallationStatusInput(std::function<InstallationStatus()> installationStatusInput);
};

} //endif namespace ArduinoOcpp

#if !defined(AO_CUSTOM_UPDATER) && !defined(AO_CUSTOM_WS)
#if defined(ESP32) || defined(ESP8266)

namespace ArduinoOcpp {
namespace EspWiFi {

FirmwareService *makeFirmwareService(Context& context);

}
}

#endif //defined(ESP32) || defined(ESP8266)
#endif //!defined(AO_CUSTOM_UPDATER) && !defined(AO_CUSTOM_WS)

#endif
