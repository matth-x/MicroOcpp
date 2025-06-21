// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Diagnostics/DiagnosticsService.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Boot/BootService.h>
#include <MicroOcpp/Model/Configuration/ConfigurationService.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlService.h>
#include <MicroOcpp/Model/SecurityEvent/SecurityEventService.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Debug.h>

#include <MicroOcpp/Operations/GetDiagnostics.h>
#include <MicroOcpp/Operations/DiagnosticsStatusNotification.h>
#include <MicroOcpp/Operations/GetLog.h>
#include <MicroOcpp/Operations/LogStatusNotification.h>

//Fetch relevant data from other modules for diagnostics
#include <MicroOcpp/Model/Availability/AvailabilityService.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Model/Transactions/TransactionService16.h>
#include <MicroOcpp/Model/Transactions/TransactionService201.h>
#include <MicroOcpp/Version.h> //for MO_ENABLE_V201
#include <MicroOcpp/Model/RemoteControl/RemoteControlDefs.h> //for MO_ENABLE_CONNECTOR_LOCK

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_DIAGNOSTICS

#define MO_DIAG_PREAMBLE_SIZE 192U
#define MO_DIAG_POSTAMBLE_SIZE 1024U

namespace MicroOcpp {

#if MO_USE_DIAGNOSTICS == MO_DIAGNOSTICS_BUILTIN_MBEDTLS_ESP32
size_t defaultDiagnosticsReader(char *buf, size_t size, void *user_data);
void defaultDiagnosticsOnClose(void *user_data);
#endif

DiagnosticsService::DiagnosticsService(Context& context) : MemoryManaged("v16/v201.Diagnostics.DiagnosticsService"), context(context), diagFileList(makeVector<String>(getMemoryTag())) {

}

DiagnosticsService::~DiagnosticsService() {
    MO_FREE(customProtocols);
    customProtocols = nullptr;
    MO_FREE(location);
    location = nullptr;
    MO_FREE(filename);
    filename = nullptr;
    MO_FREE(diagPreamble);
    diagPreamble = nullptr;
    MO_FREE(diagPostamble);
    diagPostamble = nullptr;
}

bool DiagnosticsService::setup() {

    filesystem = context.getFilesystem();
    ftpClient = context.getFtpClient();

#if MO_USE_DIAGNOSTICS == MO_DIAGNOSTICS_CUSTOM
    if (!onUpload || !onUploadStatusInput) {
        MO_DBG_ERR("need to set onUpload cb and onUploadStatusInput cb");
        return false;
    }
#else
    if ((!onUpload || !onUploadStatusInput) && ftpClient) {
        if (!filesystem) {
            MO_DBG_WARN("Security Log upload disabled (volatile mode)");
        }
    } else {
        MO_DBG_ERR("depends on FTP client");
        return false;
    }
#endif

#if MO_USE_DIAGNOSTICS == MO_DIAGNOSTICS_BUILTIN_MBEDTLS_ESP32
    if (!diagnosticsReader) {
        diagnosticsReader = defaultDiagnosticsReader;
        diagnosticsOnClose = defaultDiagnosticsOnClose;
    }
#endif //MO_USE_DIAGNOSTICS == MO_DIAGNOSTICS_BUILTIN_MBEDTLS_ESP32

    char fileTransferProtocolsBuf [sizeof("FTP,FTPS,HTTP,HTTPS,SFTP") * 2]; //dimension to fit all allowed protocols (plus some safety space)
    if (customProtocols) {
        auto ret = snprintf(fileTransferProtocolsBuf, sizeof(fileTransferProtocolsBuf), "%s", customProtocols);
        if (ret < 0 || (size_t)ret >= sizeof(fileTransferProtocolsBuf)) {
            MO_DBG_ERR("custom protocols too long");
            return false;
        }
        MO_FREE(customProtocols);
        customProtocols = nullptr;
    }
    

    ocppVersion = context.getOcppVersion();
    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {
        auto configService = context.getModel16().getConfigurationService();
        if (!configService) {
            MO_DBG_ERR("initialization error");
            return false;
        }

        auto fileTransferProtocols = configService->declareConfiguration<const char*>("SupportedFileTransferProtocols", "", MO_CONFIGURATION_VOLATILE, Mutability::ReadOnly);
        if (!fileTransferProtocols) {
            MO_DBG_ERR("initialization error");
            return false;
        }

        if (customProtocols) {
            fileTransferProtocols->setString(customProtocols);
        } else if (ftpClient) {
            fileTransferProtocols->setString("FTP,FTPS");
        }

        context.getMessageService().registerOperation("GetDiagnostics", [] (Context& context) -> Operation* {
            return new Ocpp16::GetDiagnostics(context, *context.getModel16().getDiagnosticsService());});

        context.getMessageService().registerOperation("GetLog", [] (Context& context) -> Operation* {
            return new GetLog(context, *context.getModel16().getDiagnosticsService());});

        #if MO_ENABLE_MOCK_SERVER
        context.getMessageService().registerOperation("DiagnosticsStatusNotification", nullptr, nullptr);
        context.getMessageService().registerOperation("LogStatusNotification", nullptr, nullptr);
        #endif //MO_ENABLE_MOCK_SERVER

        auto rcService = context.getModel16().getRemoteControlService();
        if (!rcService) {
            MO_DBG_ERR("initialization error");
            return false;
        }

        rcService->addTriggerMessageHandler("DiagnosticsStatusNotification", [] (Context& context) -> Operation* {
            auto diagSvc = context.getModel16().getDiagnosticsService();
            return new Ocpp16::DiagnosticsStatusNotification(diagSvc->getUploadStatus16());
        });

        rcService->addTriggerMessageHandler("LogStatusNotification", [] (Context& context) -> Operation* {
            auto diagService = context.getModel16().getDiagnosticsService();
            return new LogStatusNotification(diagService->getUploadStatus(), diagService->getRequestId());});
    }
    #endif //MO_ENABLE_V16
    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {
        auto varService = context.getModel201().getVariableService();
        if (!varService) {
            MO_DBG_ERR("initialization error");
            return false;
        }

        //if transactions can start before the BootNotification succeeds
        auto fileTransferProtocols = varService->declareVariable<const char*>("OCPPCommCtrlr", "FileTransferProtocols", "", Mutability::ReadOnly, false);
        if (!fileTransferProtocols) {
            MO_DBG_ERR("initialization error");
            return false;
        }

        if (customProtocols) {
            fileTransferProtocols->setString(customProtocols);
        } else if (ftpClient) {
            fileTransferProtocols->setString("FTP,FTPS");
        }

        context.getMessageService().registerOperation("GetLog", [] (Context& context) -> Operation* {
            return new GetLog(context, *context.getModel201().getDiagnosticsService());});

        #if MO_ENABLE_MOCK_SERVER
        context.getMessageService().registerOperation("LogStatusNotification", nullptr, nullptr);
        #endif //MO_ENABLE_MOCK_SERVER

        auto rcService = context.getModel16().getRemoteControlService();
        if (!rcService) {
            MO_DBG_ERR("initialization error");
            return false;
        }

        rcService->addTriggerMessageHandler("LogStatusNotification", [] (Context& context) -> Operation* {
            auto diagService = context.getModel201().getDiagnosticsService();
            return new LogStatusNotification(diagService->getUploadStatus(), diagService->getRequestId());});
    }
    #endif //MO_ENABLE_V201

    MO_FREE(customProtocols); //not needed anymore
    customProtocols = nullptr;

    return true;
}

void DiagnosticsService::loop() {

    if (ftpUpload) {
        if (ftpUpload->isActive()) {
            ftpUpload->loop();
        } else {
            MO_DBG_DEBUG("Deinit FTP upload");
            ftpUpload.reset();
        }
    }

    Operation *uploadStatusNotification = nullptr;

#if MO_ENABLE_V16
    if (use16impl) {
        if (getUploadStatus16() != lastReportedUploadStatus16) {
            lastReportedUploadStatus16 = getUploadStatus16();
            if (lastReportedUploadStatus16 != Ocpp16::DiagnosticsStatus::Idle) {
                uploadStatusNotification = new Ocpp16::DiagnosticsStatusNotification(lastReportedUploadStatus16);
                if (!uploadStatusNotification) {
                    MO_DBG_ERR("OOM");
                }
            }
        }
    } else
#endif
    {
        if (getUploadStatus() != lastReportedUploadStatus) {
            lastReportedUploadStatus = getUploadStatus();
            if (lastReportedUploadStatus != MO_UploadLogStatus_Idle) {
                uploadStatusNotification = new LogStatusNotification(lastReportedUploadStatus, requestId);
                if (!uploadStatusNotification) {
                    MO_DBG_ERR("OOM");
                }
            }
        }
    }

    if (uploadStatusNotification) {
        auto request = makeRequest(context, uploadStatusNotification);
        if (!request) {
            MO_DBG_ERR("OOM");
            delete uploadStatusNotification;
        } else {
            context.getMessageService().sendRequest(std::move(request));
        }
        uploadStatusNotification = nullptr;
    }

    if (runCustomUpload) {
        if (getUploadStatus() != MO_UploadLogStatus_Uploading) {
            MO_DBG_INFO("custom upload finished");
            runCustomUpload = false;
        }
        return;
    }

    auto& clock = context.getClock();

    int32_t dtNextTry;
    if (!clock.delta(clock.getUptime(), nextTry, dtNextTry)) {
        dtNextTry = -1;
    }

    if (retries > 0 && dtNextTry >= 0) {

        if (!uploadIssued) {

            bool success = false;
            switch (logType) {
                case MO_LogType_DiagnosticsLog:
                    success = uploadDiagnostics();
                    break;
                case MO_LogType_SecurityLog:
                    success = uploadSecurityLog();
                    break;
                case MO_LogType_UNDEFINED:
                    MO_DBG_ERR("undefined value");
                    success = false;
                    break;
            }

            if (success) {
                uploadIssued = true;
                uploadFailure = false;
            } else {
                MO_DBG_ERR("cannot upload via FTP. Abort");
                retries = 0;
                uploadIssued = false;
                uploadFailure = true;
                cleanUploadData();
                cleanGetLogData();
            }
        }

        if (uploadIssued) {
            if (getUploadStatus() == MO_UploadLogStatus_Uploaded) {
                //success!
                MO_DBG_DEBUG("end upload routine (by status)");
                uploadIssued = false;
                retries = 0;
                cleanUploadData();
                cleanGetLogData();
            }

            //check if maximum time elapsed or failed
            bool isUploadFailure = false;
            switch(getUploadStatus()) {
                case MO_UploadLogStatus_BadMessage:
                case MO_UploadLogStatus_NotSupportedOperation:
                case MO_UploadLogStatus_PermissionDenied:
                case MO_UploadLogStatus_UploadFailure:
                    isUploadFailure = true;
                    break;
                case MO_UploadLogStatus_Uploading:
                case MO_UploadLogStatus_Idle:
                case MO_UploadLogStatus_Uploaded:
                case MO_UploadLogStatus_AcceptedCanceled:
                    isUploadFailure = false;
                    break;
            }

            const int UPLOAD_TIMEOUT = 60;
            if (isUploadFailure || dtNextTry >= UPLOAD_TIMEOUT) {
                //maximum upload time elapsed or failed

                //either we have UploadFailed status or (NotUploaded + timeout) here
                MO_DBG_WARN("Upload timeout or failed");
                cleanUploadData();

                const int TRANSITION_DELAY = 10;
                if (retryInterval <= UPLOAD_TIMEOUT + TRANSITION_DELAY) {
                    nextTry = clock.getUptime();
                    clock.add(nextTry, TRANSITION_DELAY); //wait for another 10 seconds
                } else {
                    clock.add(nextTry, retryInterval);
                }
                retries--;

                if (retries == 0) {
                    MO_DBG_DEBUG("end upload routine (no more retry)");
                    uploadFailure = true;
                    cleanGetLogData();
                }
            }
        } //end if (uploadIssued)
    } //end try upload
}

#if MO_ENABLE_V16
bool DiagnosticsService::requestDiagnosticsUpload(const char *location, unsigned int retries, unsigned int retryInterval, Timestamp startTime, Timestamp stopTime, char filenameOut[MO_GETLOG_FNAME_SIZE]) {
    
    bool success = false;
    
    auto ret = getLog(MO_LogType_DiagnosticsLog, -1, retries, retryInterval, location, startTime, stopTime, filenameOut);
    switch (ret) {
        case MO_GetLogStatus_Accepted:
        case MO_GetLogStatus_AcceptedCanceled:
            this->use16impl = true;
            success = true;
            break;
        case MO_GetLogStatus_Rejected:
            filenameOut[0] = '\0'; //clear filename - that means that no upload will follow
            success = true;
            break;
        case MO_GetLogStatus_UNDEFINED:
            success = false;
            break;
    }
    return success;
}
#endif //MO_ENABLE_V16

MO_GetLogStatus DiagnosticsService::getLog(MO_LogType type, int requestId, int retries, unsigned int retryInterval, const char *remoteLocation, Timestamp oldestTimestamp, Timestamp latestTimestamp, char filenameOut[MO_GETLOG_FNAME_SIZE]) {

    if (runCustomUpload || this->retries > 0) {
        MO_DBG_INFO("upload still running");
        return MO_GetLogStatus_Rejected;
    }

    //clean data which outlives last GetLog
    ftpUploadStatus = MO_UploadLogStatus_Idle;

    #if MO_ENABLE_V16
    use16impl = false; //may be re-assigned in requestDiagnosticsUpload
    #endif

    //set data which is needed for custom upload and built-in upload
    this->requestId = requestId;

    auto& clock = context.getClock();

    if (onUpload && onUploadStatusInput) {
        //initiate custom upload

        char oldestTimestampStr [MO_JSONDATE_SIZE];
        if (oldestTimestamp.isDefined()) {
            if (!clock.toJsonString(oldestTimestamp, oldestTimestampStr, sizeof(oldestTimestampStr))) {
                MO_DBG_ERR("toJsonString");
                return MO_GetLogStatus_UNDEFINED;
            }
        }

        char latestTimestampStr [MO_JSONDATE_SIZE];
        if (latestTimestamp.isDefined()) {
            if (!clock.toJsonString(latestTimestamp, latestTimestampStr, sizeof(latestTimestampStr))) {
                MO_DBG_ERR("toJsonString");
                return MO_GetLogStatus_UNDEFINED;
            }
        }

        auto ret = onUpload(
                type,
                location,
                oldestTimestamp.isDefined() ? oldestTimestampStr : nullptr,
                latestTimestamp.isDefined() ? latestTimestampStr : nullptr,
                filenameOut,
                onUploadUserData);
        if (ret == MO_GetLogStatus_Accepted || ret == MO_GetLogStatus_AcceptedCanceled) {
            runCustomUpload = true;
        }
        return ret;
    }

    //initiate built-in upload

    bool hasFilename = false;
    size_t filenameSize = 0;

    MO_FREE(this->location);
    this->location = nullptr;
    size_t locationSize = strlen(location);
    this->location = static_cast<char*>(MO_MALLOC(getMemoryTag(), locationSize));
    if (!this->location) {
        MO_DBG_ERR("OOM");
        goto fail;
    }

    MO_FREE(this->filename);
    this->filename = nullptr;

    if (refreshFilename) {
        hasFilename = refreshFilename(type, filenameOut, refreshFilenameUserData);
    }

    if (!hasFilename) {
        auto ret = snprintf(filenameOut, MO_GETLOG_FNAME_SIZE, "diagnostics.log");
        if (ret < 0 || (size_t)ret >= MO_GETLOG_FNAME_SIZE) {
            MO_DBG_ERR("snprintf: %i", ret);
            goto fail;
        }
    }

    filenameSize = strlen(filenameOut) + 1;
    this->filename = static_cast<char*>(MO_MALLOC(getMemoryTag(), filenameSize));
    if (!this->filename) {
        MO_DBG_ERR("OOM");
        goto fail;
    }
    snprintf(this->filename, filenameSize, "%s", filenameOut);

    this->logType = type;
    this->retries = retries > 0 ? retries : 1;
    this->retryInterval = retryInterval > 30 ? retryInterval : 30;
    this->oldestTimestamp = oldestTimestamp;
    this->latestTimestamp = latestTimestamp;

#if MO_DBG_LEVEL >= MO_DL_INFO
    {
        char dbuf [MO_JSONDATE_SIZE] = {'\0'};
        char dbuf2 [MO_JSONDATE_SIZE] = {'\0'};
        if (oldestTimestamp.isDefined()) {
            clock.toJsonString(oldestTimestamp, dbuf, sizeof(dbuf));
        }
        if (latestTimestamp.isDefined()) {
            clock.toJsonString(latestTimestamp, dbuf2, sizeof(dbuf2));
        }

        MO_DBG_INFO("Scheduled Diagnostics upload!\n" \
                        "                  location = %s\n" \
                        "                  retries = %i" \
                        ", retryInterval = %u" \
                        "                  oldestTimestamp = %s\n" \
                        "                  latestTimestamp = %s",
                this->location,
                this->retries,
                this->retryInterval,
                dbuf,
                dbuf2);
    }
#endif

    nextTry = clock.now();
    clock.add(nextTry, 5); //wait for 5s before upload
    uploadIssued = false;
    uploadFailure = false;

#if MO_DBG_LEVEL >= MO_DL_DEBUG
    {
        char dbuf [MO_JSONDATE_SIZE] = {'\0'};
        if (nextTry.isDefined()) {
            clock.toJsonString(nextTry, dbuf, sizeof(dbuf));
        }
        MO_DBG_DEBUG("Initial try at %s", dbuf);
    }
#endif

    return MO_GetLogStatus_Accepted;
fail:
    cleanGetLogData();
    return MO_GetLogStatus_UNDEFINED;
}

int DiagnosticsService::getRequestId() {
    if (runCustomUpload || this->retries > 0) {
        return requestId;
    } else {
        return -1;
    }
}

MO_UploadLogStatus DiagnosticsService::getUploadStatus() {

    MO_UploadLogStatus status = MO_UploadLogStatus_Idle;

    if (runCustomUpload) {
        status = onUploadStatusInput(onUploadUserData);
    } else if (uploadFailure) {
        status = MO_UploadLogStatus_UploadFailure;
    } else {
        status = ftpUploadStatus;
    }
    return status;
}

Ocpp16::DiagnosticsStatus DiagnosticsService::getUploadStatus16() {

    MO_UploadLogStatus status = getUploadStatus();

    auto res = Ocpp16::DiagnosticsStatus::Idle;

    switch(status) {
        case MO_UploadLogStatus_Idle:
            res = Ocpp16::DiagnosticsStatus::Idle;
            break;
        case MO_UploadLogStatus_Uploaded:
        case MO_UploadLogStatus_AcceptedCanceled:
            res = Ocpp16::DiagnosticsStatus::Uploaded;
            break;
        case MO_UploadLogStatus_BadMessage:
        case MO_UploadLogStatus_NotSupportedOperation:
        case MO_UploadLogStatus_PermissionDenied:
        case MO_UploadLogStatus_UploadFailure:
            res = Ocpp16::DiagnosticsStatus::UploadFailed;
            break;
        case MO_UploadLogStatus_Uploading:
            res = Ocpp16::DiagnosticsStatus::Uploading;
            break;
    }
    return res;
}

void DiagnosticsService::setDiagnosticsReader(size_t (*diagnosticsReader)(char *buf, size_t size, void *user_data), void(*onClose)(void *user_data), void *user_data) {
    this->diagnosticsReader = diagnosticsReader;
    this->diagnosticsOnClose = onClose;
    this->diagnosticsUserData = user_data;
}

void DiagnosticsService::setRefreshFilename(bool (*refreshFilename)(MO_LogType type, char filenameOut[MO_GETLOG_FNAME_SIZE], void *user_data), void *user_data) {
    this->refreshFilename = refreshFilename;
    this->refreshFilenameUserData = user_data;
}

bool DiagnosticsService::setOnUpload(
        MO_GetLogStatus (*onUpload)(MO_LogType type, const char *location, const char *oldestTimestamp, const char *latestTimestamp, char filenameOut[MO_GETLOG_FNAME_SIZE], void *user_data),
        MO_UploadLogStatus (*onUploadStatusInput)(void *user_data),
        const char *customProtocols,
        void *user_data) {
    this->onUpload = onUpload;
    this->onUploadStatusInput = onUploadStatusInput;

    MO_FREE(this->customProtocols);
    this->customProtocols = nullptr;
    if (customProtocols) {
        size_t customProtocolsSize = strlen(customProtocols) + 1;
        this->customProtocols = static_cast<char*>(MO_MALLOC(getMemoryTag(), customProtocolsSize));
        if (!this->customProtocols) {
            MO_DBG_ERR("OOM");
            return false;
        }
        snprintf(this->customProtocols, customProtocolsSize, "%s", customProtocols);
    }

    this->onUploadUserData = user_data;
    return true;
}

struct DiagnosticsReaderFtwData {
    DiagnosticsService *diagService;
    int ret;
};

bool DiagnosticsService::uploadDiagnostics() {

    MO_FREE(diagPreamble);
    diagPreamble = nullptr;

    diagPreamble = static_cast<char*>(MO_MALLOC(getMemoryTag(), MO_DIAG_PREAMBLE_SIZE));
    if (!diagPreamble) {
        MO_DBG_ERR("OOM");
        cleanUploadData();
        return false;
    }
    diagPreambleLen = 0;
    diagPreambleTransferred = 0;

    diagReaderHasData = diagnosticsReader ? true : false;

    MO_FREE(diagPostamble);
    diagPostamble = nullptr;

    diagPostamble = static_cast<char*>(MO_MALLOC(getMemoryTag(), MO_DIAG_POSTAMBLE_SIZE));
    if (!diagPostamble) {
        MO_DBG_ERR("OOM");
        cleanUploadData();
        return false;
    }
    diagPostambleLen = 0;
    diagPostambleTransferred = 0;

    BootService *bootService = nullptr;
    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {
        bootService = context.getModel16().getBootService();
    }
    #endif //MO_ENABLE_V16
    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {
        bootService = context.getModel201().getBootService();
    }
    #endif //MO_ENABLE_V201

    const char *cpModel = nullptr;
    const char *fwVersion = nullptr;

    if (bootService) {
        auto bnData = bootService->getBootNotificationData();
        cpModel = bnData.chargePointModel;
        fwVersion = bnData.firmwareVersion;
    }

    char jsonDate [MO_JSONDATE_SIZE];
    if (!context.getClock().toJsonString(context.getClock().now(), jsonDate, sizeof(jsonDate))) {
        MO_DBG_ERR("internal error");
        jsonDate[0] = '\0';
    }

    int ret;

    ret = snprintf(diagPreamble, MO_DIAG_PREAMBLE_SIZE,
            "### %s Security Log%s%s\n%s\n",
            cpModel ? cpModel : "Charger",
            fwVersion ? " - v. " : "", fwVersion ? fwVersion : "",
            jsonDate);

    if (ret < 0 || (size_t)ret >= MO_DIAG_PREAMBLE_SIZE) {
        MO_DBG_ERR("snprintf: %i", ret);
        cleanUploadData();
        return false;
    }

    diagPreambleLen += (size_t)ret;

    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {
        //OCPP 1.6 specific diagnostics

        auto& model = context.getModel16();

        auto txSvc = model.getTransactionService();
        auto txSvcEvse1 = txSvc ? txSvc->getEvse(1) : nullptr;
        auto txSvcEvse2 = txSvc && model.getNumEvseId() > 2 ? txSvc->getEvse(2) : nullptr;
        auto txSvcEvse1Tx = txSvcEvse1 ? txSvcEvse1->getTransaction() : nullptr;
        auto txSvcEvse2Tx = txSvcEvse2 ? txSvcEvse2->getTransaction() : nullptr;

        auto availSvc = model.getAvailabilityService();
        auto availSvcEvse0 = availSvc ? availSvc->getEvse(0) : nullptr;
        auto availSvcEvse1 = availSvc ? availSvc->getEvse(1) : nullptr;
        auto availSvcEvse2 = availSvc ? availSvc->getEvse(2) : nullptr;

        ret = 0;

        if (ret >= 0 && (size_t)ret + diagPostambleLen < MO_DIAG_POSTAMBLE_SIZE) {
            diagPostambleLen += (size_t)ret;
            ret = snprintf(diagPostamble + diagPostambleLen, MO_DIAG_POSTAMBLE_SIZE - diagPostambleLen,
                "\n# OCPP"
                "\nclient_version=%s"
                "\nuptime=%lus"
                "%s%s"
                "%s%s"
                "%s%s"
                "\nws_status=%s"
                "%s%s"
                "%s%s"
                "%s%s"
                "%s%s"
                "%s%s"
                "%s%s"
                "%s%s"
                "%s%s"
                "\nENABLE_CONNECTOR_LOCK=%i"
                "\nENABLE_FILE_INDEX=%i"
                "\nENABLE_V201=%i"
                "\n",
                MO_VERSION,
                context.getTicksMs() / 1000UL,
                availSvcEvse0 ? "\nocpp_status_cId0=" : "", availSvcEvse0 ? mo_serializeChargePointStatus(availSvcEvse0->getStatus()) : "",
                availSvcEvse1 ? "\nocpp_status_cId1=" : "", availSvcEvse1 ? mo_serializeChargePointStatus(availSvcEvse1->getStatus()) : "",
                availSvcEvse2 ? "\nocpp_status_cId2=" : "", availSvcEvse2 ? mo_serializeChargePointStatus(availSvcEvse2->getStatus()) : "",
                context.getConnection() && context.getConnection()->isConnected() ? "connected" : "unconnected",
                txSvcEvse1 ? "\ncId1_hasTx=" : "", txSvcEvse1 ? (txSvcEvse1Tx ? "1" : "0") : "",
                txSvcEvse1Tx ? "\ncId1_txActive=" : "", txSvcEvse1Tx ? (txSvcEvse1Tx->isActive() ? "1" : "0") : "",
                txSvcEvse1Tx ? "\ncId1_txHasStarted=" : "", txSvcEvse1Tx ? (txSvcEvse1Tx->getStartSync().isRequested() ? "1" : "0") : "",
                txSvcEvse1Tx ? "\ncId1_txHasStopped=" : "", txSvcEvse1Tx ? (txSvcEvse1Tx->getStopSync().isRequested() ? "1" : "0") : "",
                txSvcEvse2 ? "\ncId2_hasTx=" : "", txSvcEvse2 ? (txSvcEvse2Tx ? "1" : "0") : "",
                txSvcEvse2Tx ? "\ncId2_txActive=" : "", txSvcEvse2Tx ? (txSvcEvse2Tx->isActive() ? "1" : "0") : "",
                txSvcEvse2Tx ? "\ncId2_txHasStarted=" : "", txSvcEvse2Tx ? (txSvcEvse2Tx->getStartSync().isRequested() ? "1" : "0") : "",
                txSvcEvse2Tx ? "\ncId2_txHasStopped=" : "", txSvcEvse2Tx ? (txSvcEvse2Tx->getStopSync().isRequested() ? "1" : "0") : "",
                MO_ENABLE_CONNECTOR_LOCK,
                MO_ENABLE_FILE_INDEX,
                MO_ENABLE_V201
                );
        }
    }
    #endif //MO_ENABLE_V16

    if (filesystem) {

        if (ret >= 0 && (size_t)ret + diagPostambleLen < MO_DIAG_POSTAMBLE_SIZE) {
            diagPostambleLen += (size_t)ret;
            ret = snprintf(diagPostamble + diagPostambleLen, MO_DIAG_POSTAMBLE_SIZE - diagPostambleLen, "\n# Filesystem\n");
        }

        DiagnosticsReaderFtwData data;
        data.diagService = this;
        data.ret = ret;

        diagFileList.clear();

        filesystem->ftw(filesystem->path_prefix, [] (const char *fname, void *user_data) -> int {
            auto& data = *reinterpret_cast<DiagnosticsReaderFtwData*>(user_data);
            auto& diagPostambleLen = data.diagService->diagPostambleLen;
            auto& diagPostamble = data.diagService->diagPostamble;
            auto& diagFileList = data.diagService->diagFileList;
            auto& ret = data.ret;

            if (ret >= 0 && (size_t)ret + diagPostambleLen < MO_DIAG_POSTAMBLE_SIZE) {
                diagPostambleLen += (size_t)ret;
                ret = snprintf(diagPostamble + diagPostambleLen, MO_DIAG_POSTAMBLE_SIZE - diagPostambleLen, "%s\n", fname);
            }
            diagFileList.emplace_back(fname);
            return 0;
        }, reinterpret_cast<void*>(&data));

        ret = data.ret;

        MO_DBG_DEBUG("discovered %zu files", diagFileList.size());
    }

    if (ret >= 0 && (size_t)ret + diagPostambleLen < MO_DIAG_POSTAMBLE_SIZE) {
        diagPostambleLen += (size_t)ret;
    } else {
        char errMsg [64];
        auto errLen = snprintf(errMsg, sizeof(errMsg), "\n[Diagnostics cut]\n");
        size_t ellipseStart = std::min(MO_DIAG_POSTAMBLE_SIZE - (size_t)errLen - 1, diagPostambleLen);
        auto ret2 = snprintf(diagPostamble + ellipseStart, MO_DIAG_POSTAMBLE_SIZE - ellipseStart, "%s", errMsg);
        diagPostambleLen += (size_t)ret2;
    }

    if (!startFtpUpload()) {
        cleanUploadData();
        return false;
    }

    return true;
}

bool DiagnosticsService::uploadSecurityLog() {

    if (!filesystem) {
        MO_DBG_ERR("depends on filesystem");
        cleanUploadData();
        return false;
    }

    MO_FREE(diagPreamble);
    diagPreamble = nullptr;

    diagPreamble = static_cast<char*>(MO_MALLOC(getMemoryTag(), MO_DIAG_PREAMBLE_SIZE));
    if (!diagPreamble) {
        MO_DBG_ERR("OOM");
        cleanUploadData();
        return false;
    }
    diagPreambleLen = 0;
    diagPreambleTransferred = 0;

    diagReaderHasData = false;

    BootService *bootService = nullptr;
    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {
        bootService = context.getModel16().getBootService();
    }
    #endif //MO_ENABLE_V16
    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {
        bootService = context.getModel201().getBootService();
    }
    #endif //MO_ENABLE_V201

    auto cpModel = makeString(getMemoryTag());
    auto fwVersion = makeString(getMemoryTag());

    if (bootService) {
        auto bnData = bootService->getBootNotificationData();
        cpModel = bnData.chargePointModel;
        fwVersion = bnData.firmwareVersion;
    }

    char jsonDate [MO_JSONDATE_SIZE];
    if (!context.getClock().toJsonString(context.getClock().now(), jsonDate, sizeof(jsonDate))) {
        MO_DBG_ERR("internal error");
        jsonDate[0] = '\0';
    }

    int ret;

    ret = snprintf(diagPreamble, MO_DIAG_PREAMBLE_SIZE,
            "### %s Hardware Diagnostics%s%s\n%s\n",
            cpModel.c_str(),
            fwVersion.empty() ? "" : " - v. ", fwVersion.c_str(),
            jsonDate);

    if (ret < 0 || (size_t)ret >= MO_DIAG_PREAMBLE_SIZE) {
        MO_DBG_ERR("snprintf: %i", ret);
        cleanUploadData();
        return false;
    }

    diagPreambleLen += (size_t)ret;

    //load Security Log index structure from flash (fileNrBegin denotes first file, fileNrEnd denots index after last file)

    unsigned int fileNrBegin = 0, fileNrEnd = 0;

    if (!FilesystemUtils::loadRingIndex(filesystem, MO_SECLOG_FN_PREFIX, MO_SECLOG_INDEX_MAX, &fileNrBegin, &fileNrEnd)) {
        MO_DBG_ERR("failed to load security event log");
        cleanUploadData();
        return false;
    }

    DiagnosticsReaderFtwData data;
    data.diagService = this;

    diagFileList.clear();

    int ret_ftw = filesystem->ftw(filesystem->path_prefix, [] (const char *fname, void *user_data) -> int {
        auto& data = *reinterpret_cast<DiagnosticsReaderFtwData*>(user_data);
        auto& diagFileList = data.diagService->diagFileList;

        if (!strncmp(fname, MO_SECLOG_FN_PREFIX, sizeof(MO_SECLOG_FN_PREFIX) - 1)) {
            //fname starts with "slog-"
            diagFileList.emplace_back(fname);
        }

        return 0;
    }, reinterpret_cast<void*>(&data));

    if (ret_ftw != 0) {
        MO_DBG_ERR("ftw: %i", ret_ftw);
        cleanUploadData();
        return false;
    }

    MO_DBG_DEBUG("discovered %zu files", diagFileList.size());

    if (!startFtpUpload()) {
        cleanUploadData();
        return false;
    }

    return true;
}

bool DiagnosticsService::startFtpUpload() {

    size_t locationLen = strlen(location);
    size_t filenameLen = strlen(filename);
    size_t urlSize = locationLen +
                    1 + //separating '/' between location and filename
                    filenameLen  +
                    1; //terminating 0

    char *url = static_cast<char*>(MO_MALLOC(getMemoryTag(), urlSize));
    if (!url) {
        MO_DBG_ERR("OOM");
        return false;
    }

    if (locationLen > 0 && filenameLen > 0 && location[locationLen - 1] != '/') {
        snprintf(url, urlSize, "%s/%s", location, filename);
    } else {
        snprintf(url, urlSize, "%s%s", location, filename);
    }

    this->ftpUpload = ftpClient->postFile(url,
        [this] (unsigned char *buf, size_t size) -> size_t {
            size_t written = 0;
            if (written < size && diagPreambleTransferred < diagPreambleLen) {
                size_t writeLen = std::min(size - written, diagPreambleLen - diagPreambleTransferred);
                memcpy(buf + written, diagPreamble + diagPreambleTransferred, writeLen);
                diagPreambleTransferred += writeLen;
                written += writeLen;
            }

            while (written < size && diagReaderHasData && diagnosticsReader) {
                size_t writeLen = diagnosticsReader((char*)buf + written, size - written, diagnosticsUserData);
                if (writeLen == 0) {
                    diagReaderHasData = false;
                }
                written += writeLen;
            }

            if (written < size && diagPostambleTransferred < diagPostambleLen) {
                size_t writeLen = std::min(size - written, diagPostambleLen - diagPostambleTransferred);
                memcpy(buf + written, diagPostamble + diagPostambleTransferred, writeLen);
                diagPostambleTransferred += writeLen;
                written += writeLen;
            }

            while (written < size && !diagFileList.empty() && filesystem) {

                char fpath [MO_MAX_PATH_SIZE];
                if (!FilesystemUtils::printPath(filesystem, fpath, sizeof(fpath), diagFileList.back().c_str())) {
                    MO_DBG_ERR("path error: %s", diagFileList.back().c_str());
                    diagFileList.pop_back();
                    continue;
                }

                if (auto file = filesystem->open(fpath, "r")) {

                    if (diagFilesBackTransferred == 0) {
                        char fileHeading [30 + MO_MAX_PATH_SIZE];
                        auto writeLen = snprintf(fileHeading, sizeof(fileHeading), "\n\n# File %s:\n", diagFileList.back().c_str());
                        if (writeLen < 0 || (size_t)writeLen >= sizeof(fileHeading)) {
                            MO_DBG_ERR("fn error: %i", writeLen);
                            diagFileList.pop_back();
                            filesystem->close(file);
                            continue;
                        }
                        if (writeLen + written > size || //heading doesn't fit anymore, return with a bit unused buffer space and print heading the next time
                                writeLen + written == size) { //filling the buffer up exactly would mean that no file payload is written and this head gets printed again
                            
                            MO_DBG_DEBUG("upload diag chunk (%zuB)", written);
                            filesystem->close(file);
                            return written;
                        }

                        memcpy(buf + written, fileHeading, (size_t)writeLen);
                        written += (size_t)writeLen;
                        filesystem->close(file);
                    }

                    filesystem->seek(file, diagFilesBackTransferred);
                    size_t writeLen = filesystem->read(file, (char*)buf + written, size - written);

                    if (writeLen < size - written) {
                        diagFileList.pop_back();
                    }
                    written += writeLen;
                } else {
                    MO_DBG_ERR("could not open file: %s", fpath);
                    diagFileList.pop_back();
                }
            }

            MO_DBG_DEBUG("upload diag chunk (%zuB)", written);
            return written;
        },
        [this] (MO_FtpCloseReason reason) -> void {
            if (reason == MO_FtpCloseReason_Success) {
                MO_DBG_INFO("FTP upload success");
                this->ftpUploadStatus = MO_UploadLogStatus_Uploaded;
            } else {
                MO_DBG_INFO("FTP upload failure (%i)", reason);
                this->ftpUploadStatus = MO_UploadLogStatus_UploadFailure;
            }

            if (diagnosticsOnClose) {
                diagnosticsOnClose(diagnosticsUserData);
            }
        });

    MO_FREE(url);
    url = nullptr;

    if (!this->ftpUpload) {
        MO_DBG_ERR("OOM");
        return false;
    }

    this->ftpUploadStatus = MO_UploadLogStatus_Uploading;
    return true;
}

void DiagnosticsService::cleanUploadData() {
    ftpUpload.reset();

    MO_FREE(diagPreamble);
    diagPreamble = nullptr;
    diagPreambleLen = 0;
    diagPreambleTransferred = 0;

    diagReaderHasData = false;

    MO_FREE(diagPostamble);
    diagPostamble = nullptr;
    diagPostambleLen = 0;
    diagPostambleTransferred = 0;

    diagFileList.clear();
    diagFilesBackTransferred = 0;
}

void DiagnosticsService::cleanGetLogData() {
    logType = MO_LogType_UNDEFINED;
    requestId = -1;
    MO_FREE(location);
    location = nullptr;
    MO_FREE(filename);
    filename = nullptr;
    retries = 0;
    retryInterval = 0;
    oldestTimestamp = Timestamp();
    latestTimestamp = Timestamp();
}

void DiagnosticsService::setFtpServerCert(const char *cert) {
    this->ftpServerCert = cert;
}

} //namespace MicroOcpp

#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_DIAGNOSTICS

#if MO_USE_DIAGNOSTICS == MO_DIAGNOSTICS_BUILTIN_MBEDTLS_ESP32

#include "esp_heap_caps.h"
#include <LittleFS.h>

namespace MicroOcpp {

bool g_diagsSent = false;

size_t defaultDiagnosticsReader(char *buf, size_t size, void*) {
    if (!g_diagsSent) {
        g_diagsSent = true;
        int ret = snprintf(buf, size,
            "\n# Memory\n"
            "freeHeap=%zu\n"
            "minHeap=%zu\n"
            "maxAllocHeap=%zu\n"
            "LittleFS_used=%zu\n"
            "LittleFS_total=%zu\n",
            heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
            heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT),
            heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT),
            LittleFS.usedBytes(),
            LittleFS.totalBytes()
        );
        if (ret < 0 || (size_t)ret >= size) {
            MO_DBG_ERR("snprintf: %i", ret);
            return 0;
        }
        return (size_t)ret;
    }
    return 0;
}

void defaultDiagnosticsOnClose(void*) {
    g_diagsSent = false;
};

} //namespace MicroOcpp

#endif //MO_USE_DIAGNOSTICS
