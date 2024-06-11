// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef DIAGNOSTICSSERVICE_H
#define DIAGNOSTICSSERVICE_H

#include <functional>
#include <memory>
#include <string>
#include <deque>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Core/Ftp.h>
#include <MicroOcpp/Model/Diagnostics/DiagnosticsStatus.h>

namespace MicroOcpp {

enum class UploadStatus {
    NotUploaded,
    Uploaded,
    UploadFailed
};

class Context;
class Request;
class FilesystemAdapter;

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

    std::unique_ptr<FtpUpload> ftpUpload;
    UploadStatus ftpUploadStatus = UploadStatus::NotUploaded;
    const char *ftpServerCert = nullptr;
    std::shared_ptr<FilesystemAdapter> filesystem;
    char *diagPreamble = nullptr;
    size_t diagPreambleLen = 0;
    size_t diagPreambleTransferred = 0;
    bool diagReaderHasData = false;
    char *diagPostamble = nullptr;
    size_t diagPostambleLen = 0;
    size_t diagPostambleTransferred = 0;
    std::deque<std::string> diagFileList;
    size_t diagFilesFrontTransferred = 0;

    std::unique_ptr<Request> getDiagnosticsStatusNotification();

    Ocpp16::DiagnosticsStatus lastReportedStatus = Ocpp16::DiagnosticsStatus::Idle;

public:
    DiagnosticsService(Context& context);
    ~DiagnosticsService();

    void loop();

    //timestamps before year 2021 will be treated as "undefined"
    //returns empty std::string if onUpload is missing or upload cannot be scheduled for another reason
    //returns fileName of diagnostics file to be uploaded if upload has been scheduled
    std::string requestDiagnosticsUpload(const char *location, unsigned int retries = 1, unsigned int retryInterval = 0, Timestamp startTime = Timestamp(), Timestamp stopTime = Timestamp());

    Ocpp16::DiagnosticsStatus getDiagnosticsStatus();

    void setRefreshFilename(std::function<std::string()> refreshFn); //refresh a new filename which will be used for the subsequent upload tries

    /*
     * Sets the diagnostics data reader. When the server sends a GetDiagnostics operation, then MO will open an FTP
     * connection to the FTP server and upload a diagnostics file. MO automatically creates a small report about
     * the OCPP-related status data + it uploads the contents of the OCPP directory. In addition to the automatic
     * report, MO also sends all data provided by the custom diagnosticsReader. Use the diagnosticsReader to add
     * all data which could be helpful for troubleshooting, i.e.
     *     - internal status variables, or state machine states
     *     - error trip counters
     *     - current sensor readings and all GPIO values
     *     - Heap statistics, flash memory statistics
     *     - and more. The more the better
     *
     * MO calls the diagnosticsReader output buffer `buf` and the bufsize `size`. Write at most `size` bytes and
     * return the number of bytes actually written (without terminating zero-byte). It's not necessary to append
     * a terminating zero, MO will ignore any data after the string. To end the reading process, return 0.
     *
     * Note that this function only works if MO_ENABLE_MBEDTLS=1, or MO has been configured with a custom FTP client
     */
    void setDiagnosticsReader(std::function<size_t(char *buf, size_t size)> diagnosticsReader, std::function<void()> onClose, std::shared_ptr<FilesystemAdapter> filesystem);

    void setFtpServerCert(const char *cert); //zero-copy mode, i.e. cert must outlive MO

    void setOnUpload(std::function<bool(const char *location, Timestamp &startTime, Timestamp &stopTime)> onUpload);

    void setOnUploadStatusInput(std::function<UploadStatus()> uploadStatusInput);
};

} //end namespace MicroOcpp

#if !defined(MO_CUSTOM_DIAGNOSTICS)

#if MO_PLATFORM == MO_PLATFORM_ARDUINO && defined(ESP32) && MO_ENABLE_MBEDTLS

namespace MicroOcpp {
std::unique_ptr<DiagnosticsService> makeDefaultDiagnosticsService(Context& Context);
}

#endif //MO_PLATFORM == MO_PLATFORM_ARDUINO && defined(ESP32) && MO_ENABLE_MBEDTLS
#endif //!defined(MO_CUSTOM_DIAGNOSTICS)

#endif
