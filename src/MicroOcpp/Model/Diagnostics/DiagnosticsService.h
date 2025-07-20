// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef DIAGNOSTICSSERVICE_H
#define DIAGNOSTICSSERVICE_H

#include <functional>
#include <memory>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Core/Ftp.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Model/Diagnostics/Diagnostics.h>
#include <MicroOcpp/Version.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_DIAGNOSTICS

#define MO_DIAGNOSTICS_CUSTOM 0
#define MO_DIAGNOSTICS_BUILTIN_MBEDTLS 1
#define MO_DIAGNOSTICS_BUILTIN_MBEDTLS_ESP32 2

#ifndef MO_USE_DIAGNOSTICS
#ifdef MO_CUSTOM_DIAGNOSTICS
#define MO_USE_DIAGNOSTICS MO_DIAGNOSTICS_CUSTOM
#elif MO_PLATFORM == MO_PLATFORM_ARDUINO && defined(ESP32) && MO_ENABLE_MBEDTLS
#define MO_USE_DIAGNOSTICS MO_DIAGNOSTICS_BUILTIN_MBEDTLS_ESP32
#elif MO_ENABLE_MBEDTLS
#define MO_USE_DIAGNOSTICS MO_DIAGNOSTICS_BUILTIN_MBEDTLS
#else
#define MO_USE_DIAGNOSTICS MO_DIAGNOSTICS_CUSTOM
#endif
#endif //MO_USE_DIAGNOSTICS

namespace MicroOcpp {

class Context;
class Request;
class FtpClient;

class DiagnosticsService : public MemoryManaged {
private:
    Context& context;

    //platform requirements for built-in logs upload
    MO_FilesystemAdapter *filesystem = nullptr;
    FtpClient *ftpClient = nullptr;

    //custom logs upload (bypasses built-in implementation)
    MO_GetLogStatus (*onUpload)(MO_LogType type, const char *location, const char *oldestTimestamp, const char *latestTimestamp, char filenameOut[MO_GETLOG_FNAME_SIZE], void *onUploadUserData) = nullptr;
    MO_UploadLogStatus (*onUploadStatusInput)(void *onUploadUserData) = nullptr;
    void *onUploadUserData = nullptr;
    char *customProtocols = nullptr;

    int ocppVersion = -1;

    size_t (*diagnosticsReader)(char *buf, size_t size, void *diagnosticsReaderUserData) = nullptr;
    void(*diagnosticsOnClose)(void *diagnosticsReaderUserData) = nullptr;
    void *diagnosticsUserData = nullptr;

    //override filename for built-in logs upload
    bool (*refreshFilename)(MO_LogType type, char filenameOut[MO_GETLOG_FNAME_SIZE], void *refreshFilenameUserData) = nullptr;
    void *refreshFilenameUserData = nullptr;

    //server request data
    MO_LogType logType = MO_LogType_UNDEFINED;
    int requestId = -1;
    char *location = nullptr;
    char *filename = nullptr;
    int retries = -1;
    int retryInterval = 0;
    Timestamp oldestTimestamp;
    Timestamp latestTimestamp;

    //internal status
    Timestamp nextTry;
    bool uploadIssued = false;
    bool uploadFailure = false;
#if MO_ENABLE_V16
    bool use16impl = false; //if to use original 1.6 implementation instead of Security Whitepaper
#endif //MO_ENABLE_V16
    bool runCustomUpload = false;

    //built-in logs upload data
    std::unique_ptr<FtpUpload> ftpUpload;
    MO_UploadLogStatus ftpUploadStatus = MO_UploadLogStatus_Idle;
    const char *ftpServerCert = nullptr;
    char *diagPreamble = nullptr;
    size_t diagPreambleLen = 0;
    size_t diagPreambleTransferred = 0;
    bool diagReaderHasData = false;
    char *diagPostamble = nullptr;
    size_t diagPostambleLen = 0;
    size_t diagPostambleTransferred = 0;
    Vector<String> diagFileList;
    size_t diagFilesBackTransferred = 0;

    MO_UploadLogStatus lastReportedUploadStatus = MO_UploadLogStatus_Idle;
    #if MO_ENABLE_V16
    v16::DiagnosticsStatus lastReportedUploadStatus16 = v16::DiagnosticsStatus::Idle;
    #endif //MO_ENABLE_V16

    bool uploadDiagnostics(); //prepare diags and start FTP upload
    bool uploadSecurityLog(); //prepare sec logs and start FTP upload
    bool startFtpUpload(); //do FTP upload
    void cleanUploadData(); //clean data which is set per FTP upload attempt
    void cleanGetLogData(); //clean data which is set per GetLog request

public:
    DiagnosticsService(Context& context);
    ~DiagnosticsService();

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
    void setDiagnosticsReader(size_t (*diagnosticsReader)(char *buf, size_t size, void *user_data), void(*onClose)(void *user_data), void *user_data);

    void setFtpServerCert(const char *cert); //zero-copy mode, i.e. cert must outlive MO

    //override default logs file name ("diagnostics.log"). Only applies to built-in diagnostics upload
    void setRefreshFilename(bool (*refreshFilename)(MO_LogType type, char filenameOut[MO_GETLOG_FNAME_SIZE], void *user_data), void *user_data);

    /* Bypass built-in upload
     * `onUpload`: hook function for diagnostics upload
     * `onUploadStatusInput`: status getter cb which MO will execute to control custom onUpload and notify server
     * `protocols`: CSV list for supported protocols. Possible values: FTP, FTPS, HTTP, HTTPS, SFTP
     * `user_data`: implementer-defined pointer */
    bool setOnUpload(
            MO_GetLogStatus (*onUpload)(MO_LogType type, const char *location, const char *oldestTimestamp, const char *latestTimestamp, char filenameOut[MO_GETLOG_FNAME_SIZE], void *user_data),
            MO_UploadLogStatus (*onUploadStatusInput)(void *user_data),
            const char *protocols,
            void *user_data);

    bool setup();

    void loop();

#if MO_ENABLE_V16
    //timestamps before year 2021 will be treated as "undefined"
    //returns empty std::string if onUpload is missing or upload cannot be scheduled for another reason
    //returns fileName of diagnostics file to be uploaded if upload has been scheduled
    bool requestDiagnosticsUpload(const char *location, unsigned int retries, int retryInterval, Timestamp startTime, Timestamp stopTime, char filenameOut[MO_GETLOG_FNAME_SIZE]);
#endif //MO_ENABLE_V16

    MO_GetLogStatus getLog(MO_LogType type, int requestId, int retries, int retryInterval, const char *remoteLocation, Timestamp oldestTimestamp, Timestamp latestTimestamp, char filenameOut[MO_GETLOG_FNAME_SIZE]);

    //for TriggerMessage
    int getRequestId();
    MO_UploadLogStatus getUploadStatus();
    #if MO_ENABLE_V16
    v16::DiagnosticsStatus getUploadStatus16();
    #endif //MO_ENABLE_V16
};

} //namespace MicroOcpp
#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_DIAGNOSTICS
#endif
