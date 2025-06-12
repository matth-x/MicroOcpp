// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef FIRMWARESERVICE_H
#define FIRMWARESERVICE_H

#include <functional>
#include <memory>

#include <MicroOcpp/Model/Configuration/Configuration.h>
#include <MicroOcpp/Model/FirmwareManagement/FirmwareStatus.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Core/Ftp.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Version.h>

#define MO_FW_UPDATER_CUSTOM 0
#define MO_FW_UPDATER_BUILTIN_ESP32 1
#define MO_FW_UPDATER_BUILTIN_ESP8266 2

#ifndef MO_USE_FW_UPDATER
#ifdef MO_CUSTOM_UPDATER
#define MO_USE_FW_UPDATER MO_FW_UPDATER_CUSTOM
#elif MO_PLATFORM == MO_PLATFORM_ARDUINO && defined(ESP32) && MO_ENABLE_MBEDTLS
#define MO_USE_FW_UPDATER MO_FW_UPDATER_BUILTIN_ESP32
#elif MO_PLATFORM == MO_PLATFORM_ARDUINO && defined(ESP8266)
#define MO_USE_FW_UPDATER MO_FW_UPDATER_BUILTIN_ESP8266
#endif
#endif //MO_USE_FW_UPDATER

#if MO_ENABLE_V16 && MO_ENABLE_FIRMWAREMANAGEMENT

namespace MicroOcpp {

class Context;
class Clock;
class Request;

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

namespace Ocpp16 {

class FirmwareService : public MemoryManaged {
private:
    Context& context;
    Clock& clock;

    char *buildNumber = nullptr;
    bool notifyFirmwareUpdate = false;

    std::function<DownloadStatus()> downloadStatusInput;
    bool downloadIssued = false;

    std::unique_ptr<FtpDownload> ftpDownload;
    DownloadStatus ftpDownloadStatus = DownloadStatus::NotDownloaded;
    const char *ftpServerCert = nullptr;

    std::function<InstallationStatus()> installationStatusInput;
    bool installationIssued = false;

    FirmwareStatus lastReportedStatus = FirmwareStatus::Idle;

    char *location = nullptr;
    Timestamp retreiveDate;
    unsigned int retries = 0;
    unsigned int retryInterval = 0;

    std::function<bool(const char *location)> onDownload;
    std::function<bool(const char *location)> onInstall;

    int32_t delayTransition = 0;
    Timestamp timestampTransition;

    enum class UpdateStage {
        Idle,
        AwaitDownload,
        Downloading,
        AfterDownload,
        AwaitInstallation,
        Installing,
        Installed,
        InternalError
    } stage = UpdateStage::Idle;

    void resetStage();

    std::unique_ptr<Request> getFirmwareStatusNotification();

public:
    FirmwareService(Context& context);

    ~FirmwareService();

    bool setBuildNumber(const char *buildNumber);

    /*
     * Sets the firmware writer. During the UpdateFirmware process, MO will use an FTP client to download the firmware and forward
     * the binary data to `firmwareWriter`. The binary data comes in chunks. MO executes `firmwareWriter` with `buf` containing the
     * next chunk of FW data and `size` being the chunk size. `firmwareWriter` must return the number of bytes written, whereas
     * the result can be between 1 and `size`, and 0 aborts the download. MO executes `onClose` with the reason why the connection
     * has been closed. If the download hasn't been successful, MO will abort the update routine in any case.
     * 
     * Note that this function only works if MO_ENABLE_MBEDTLS=1, or MO has been configured with a custom FTP client
     */
    void setDownloadFileWriter(std::function<size_t(const unsigned char *buf, size_t size)> firmwareWriter, std::function<void(MO_FtpCloseReason)> onClose);

    void setFtpServerCert(const char *cert); //zero-copy mode, i.e. cert must outlive MO

    /*
     * Manual alternative for FTP download handler `setDownloadFileWriter`
     */
    void setOnDownload(std::function<bool(const char *location)> onDownload);

    void setDownloadStatusInput(std::function<DownloadStatus()> downloadStatusInput);

    void setOnInstall(std::function<bool(const char *location)> onInstall);

    void setInstallationStatusInput(std::function<InstallationStatus()> installationStatusInput);

    bool setup();

    void loop();

    bool scheduleFirmwareUpdate(const char *location, Timestamp retreiveDate, unsigned int retries = 1, unsigned int retryInterval = 0);

    FirmwareStatus getFirmwareStatus();
};

} //namespace Ocpp16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16 && MO_ENABLE_FIRMWAREMANAGEMENT
#endif
