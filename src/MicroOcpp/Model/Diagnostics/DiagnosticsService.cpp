// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Diagnostics/DiagnosticsService.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Debug.h>

#include <MicroOcpp/Operations/GetDiagnostics.h>
#include <MicroOcpp/Operations/DiagnosticsStatusNotification.h>

//Fetch relevant data from other modules for diagnostics
#include <MicroOcpp/Model/Boot/BootService.h>
#include <MicroOcpp/Operations/StatusNotification.h> //for serializing ChargePointStatus
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Version.h> //for MO_ENABLE_V201
#include <MicroOcpp/Model/ConnectorBase/UnlockConnectorResult.h> //for MO_ENABLE_CONNECTOR_LOCK

using namespace MicroOcpp;
using Ocpp16::DiagnosticsStatus;

DiagnosticsService::DiagnosticsService(Context& context) : MemoryManaged("v16.Diagnostics.DiagnosticsService"), context(context), location(makeString(getMemoryTag())), diagFileList(makeVector<String>(getMemoryTag())) {

    context.getOperationRegistry().registerOperation("GetDiagnostics", [this] () {
        return new Ocpp16::GetDiagnostics(*this);});

    //Register message handler for TriggerMessage operation
    context.getOperationRegistry().registerOperation("DiagnosticsStatusNotification", [this] () {
        return new Ocpp16::DiagnosticsStatusNotification(getDiagnosticsStatus());});
}

DiagnosticsService::~DiagnosticsService() {
    MO_FREE(diagPreamble);
    MO_FREE(diagPostamble);
}

void DiagnosticsService::loop() {

    if (ftpUpload && ftpUpload->isActive()) {
        ftpUpload->loop();
    }

    if (ftpUpload) {
        if (ftpUpload->isActive()) {
            ftpUpload->loop();
        } else {
            MO_DBG_DEBUG("Deinit FTP upload");
            ftpUpload.reset();
        }
    }

    auto notification = getDiagnosticsStatusNotification();
    if (notification) {
        context.initiateRequest(std::move(notification));
    }

    const auto& timestampNow = context.getModel().getClock().now();
    if (retries > 0 && timestampNow >= nextTry) {

        if (!uploadIssued) {
            if (onUpload != nullptr) {
                MO_DBG_DEBUG("Call onUpload");
                onUpload(location.c_str(), startTime, stopTime);
                uploadIssued = true;
                uploadFailure = false;
            } else {
                MO_DBG_ERR("onUpload must be set! (see setOnUpload). Will abort");
                retries = 0;
                uploadIssued = false;
                uploadFailure = true;
            }
        }

        if (uploadIssued) {
            if (uploadStatusInput != nullptr && uploadStatusInput() == UploadStatus::Uploaded) {
                //success!
                MO_DBG_DEBUG("end upload routine (by status)");
                uploadIssued = false;
                retries = 0;
            }

            //check if maximum time elapsed or failed
            const int UPLOAD_TIMEOUT = 60;
            if (timestampNow - nextTry >= UPLOAD_TIMEOUT
                    || (uploadStatusInput != nullptr && uploadStatusInput() == UploadStatus::UploadFailed)) {
                //maximum upload time elapsed or failed

                if (uploadStatusInput == nullptr) {
                    //No way to find out if failed. But maximum time has elapsed. Assume success
                    MO_DBG_DEBUG("end upload routine (by timer)");
                    uploadIssued = false;
                    retries = 0;
                } else {
                    //either we have UploadFailed status or (NotUploaded + timeout) here
                    MO_DBG_WARN("Upload timeout or failed");
                    ftpUpload.reset();

                    const int TRANSITION_DELAY = 10;
                    if (retryInterval <= UPLOAD_TIMEOUT + TRANSITION_DELAY) {
                        nextTry = timestampNow;
                        nextTry += TRANSITION_DELAY; //wait for another 10 seconds
                    } else {
                        nextTry += retryInterval;
                    }
                    retries--;

                    if (retries == 0) {
                        MO_DBG_DEBUG("end upload routine (no more retry)");
                        uploadFailure = true;
                    }
                }
            }
        } //end if (uploadIssued)
    } //end try upload
}

//timestamps before year 2021 will be treated as "undefined"
String DiagnosticsService::requestDiagnosticsUpload(const char *location, unsigned int retries, unsigned int retryInterval, Timestamp startTime, Timestamp stopTime) {
    if (onUpload == nullptr) {
        return makeString(getMemoryTag());
    }

    String fileName;
    if (refreshFilename) {
        fileName = refreshFilename().c_str();
    } else {
        fileName = "diagnostics.log";
    }

    this->location.reserve(strlen(location) + 1 + fileName.size());

    this->location = location;

    if (!this->location.empty() && this->location.back() != '/') {
        this->location.append("/");
    }
    this->location.append(fileName.c_str());

    this->retries = retries;
    this->retryInterval = retryInterval;
    this->startTime = startTime;
    
    Timestamp stopMin = Timestamp(2021,0,0,0,0,0);
    if (stopTime >= stopMin) {
        this->stopTime = stopTime;
    } else {
        auto newStop = context.getModel().getClock().now();
        newStop += 3600 * 24 * 365; //set new stop time one year in future
        this->stopTime = newStop;
    }
    
#if MO_DBG_LEVEL >= MO_DL_INFO
    {
        char dbuf [JSONDATE_LENGTH + 1] = {'\0'};
        char dbuf2 [JSONDATE_LENGTH + 1] = {'\0'};
        this->startTime.toJsonString(dbuf, JSONDATE_LENGTH + 1);
        this->stopTime.toJsonString(dbuf2, JSONDATE_LENGTH + 1);

        MO_DBG_INFO("Scheduled Diagnostics upload!\n" \
                        "                  location = %s\n" \
                        "                  retries = %i" \
                        ", retryInterval = %u" \
                        "                  startTime = %s\n" \
                        "                  stopTime = %s",
                this->location.c_str(),
                this->retries,
                this->retryInterval,
                dbuf,
                dbuf2);
    }
#endif

    nextTry = context.getModel().getClock().now();
    nextTry += 5; //wait for 5s before upload
    uploadIssued = false;

#if MO_DBG_LEVEL >= MO_DL_DEBUG
    {
        char dbuf [JSONDATE_LENGTH + 1] = {'\0'};
        nextTry.toJsonString(dbuf, JSONDATE_LENGTH + 1);
        MO_DBG_DEBUG("Initial try at %s", dbuf);
    }
#endif

    return fileName;
}

DiagnosticsStatus DiagnosticsService::getDiagnosticsStatus() {
    if (uploadFailure) {
        return DiagnosticsStatus::UploadFailed;
    }

    if (uploadIssued) {
        if (uploadStatusInput != nullptr) {
            switch (uploadStatusInput()) {
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

std::unique_ptr<Request> DiagnosticsService::getDiagnosticsStatusNotification() {

    if (getDiagnosticsStatus() != lastReportedStatus) {
        lastReportedStatus = getDiagnosticsStatus();
        if (lastReportedStatus != DiagnosticsStatus::Idle) {
            Operation *diagNotificationMsg = new Ocpp16::DiagnosticsStatusNotification(lastReportedStatus);
            auto diagNotification = makeRequest(diagNotificationMsg);
            return diagNotification;
        }
    }

    return nullptr;
}

void DiagnosticsService::setRefreshFilename(std::function<std::string()> refreshFn) {
    this->refreshFilename = refreshFn;
}

void DiagnosticsService::setOnUpload(std::function<bool(const char *location, Timestamp &startTime, Timestamp &stopTime)> onUpload) {
    this->onUpload = onUpload;
}

void DiagnosticsService::setOnUploadStatusInput(std::function<UploadStatus()> uploadStatusInput) {
    this->uploadStatusInput = uploadStatusInput;
}

void DiagnosticsService::setDiagnosticsReader(std::function<size_t(char *buf, size_t size)> diagnosticsReader, std::function<void()> onClose, std::shared_ptr<FilesystemAdapter> filesystem) {

    this->onUpload = [this, diagnosticsReader, onClose, filesystem] (const char *location, Timestamp &startTime, Timestamp &stopTime) -> bool {

        auto ftpClient = context.getFtpClient();
        if (!ftpClient) {
            MO_DBG_ERR("FTP client not set");
            this->ftpUploadStatus = UploadStatus::UploadFailed;
            return false;
        }

        const size_t diagPreambleSize = 128;
        diagPreamble = static_cast<char*>(MO_MALLOC(getMemoryTag(), diagPreambleSize));
        if (!diagPreamble) {
            MO_DBG_ERR("OOM");
            this->ftpUploadStatus = UploadStatus::UploadFailed;
            return false;
        }
        diagPreambleLen = 0;
        diagPreambleTransferred = 0;

        diagReaderHasData = diagnosticsReader ? true : false;

        const size_t diagPostambleSize = 1024;
        diagPostamble = static_cast<char*>(MO_MALLOC(getMemoryTag(), diagPostambleSize));
        if (!diagPostamble) {
            MO_DBG_ERR("OOM");
            this->ftpUploadStatus = UploadStatus::UploadFailed;
            MO_FREE(diagPreamble);
            return false;
        }
        diagPostambleLen = 0;
        diagPostambleTransferred = 0;

        auto& model = context.getModel();

        auto cpModel = makeString(getMemoryTag());
        auto fwVersion = makeString(getMemoryTag());

        if (auto bootService = model.getBootService()) {
            if (auto cpCreds = bootService->getChargePointCredentials()) {
                cpModel = (*cpCreds)["chargePointModel"] | "Charger";
                fwVersion = (*cpCreds)["firmwareVersion"] | "";
            }
        }

        char jsonDate [JSONDATE_LENGTH + 1];
        model.getClock().now().toJsonString(jsonDate, sizeof(jsonDate));

        int ret;

        ret = snprintf(diagPreamble, diagPreambleSize,
                "### %s Hardware Diagnostics%s%s\n%s\n",
                cpModel.c_str(),
                fwVersion.empty() ? "" : " - v. ", fwVersion.c_str(),
                jsonDate);

        if (ret < 0 || (size_t)ret >= diagPreambleSize) {
            MO_DBG_ERR("snprintf: %i", ret);
            this->ftpUploadStatus = UploadStatus::UploadFailed;
            MO_FREE(diagPreamble);
            MO_FREE(diagPostamble);
            return false;
        }

        diagPreambleLen += (size_t)ret;

        Connector *connector0 = model.getConnector(0);
        Connector *connector1 = model.getConnector(1);
        Transaction *connector1Tx = connector1 ? connector1->getTransaction().get() : nullptr;
        Connector *connector2 = model.getNumConnectors() > 2 ? model.getConnector(2) : nullptr;
        Transaction *connector2Tx = connector2 ? connector2->getTransaction().get() : nullptr;

        ret = 0;

        if (ret >= 0 && (size_t)ret + diagPostambleLen < diagPostambleSize) {
            diagPostambleLen += (size_t)ret;
            ret = snprintf(diagPostamble + diagPostambleLen, diagPostambleSize - diagPostambleLen,
                "\n# OCPP"
                "\nclient_version=%s"
                "\nuptime=%lus"
                "%s%s"
                "%s%s"
                "%s%s"
                "\nws_status=%s"
                "\nws_last_conn=%lus"
                "\nws_last_recv=%lus"
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
                mocpp_tick_ms() / 1000UL,
                connector0 ? "\nocpp_status_cId0=" : "", connector0 ? cstrFromOcppEveState(connector0->getStatus()) : "",
                connector1 ? "\nocpp_status_cId1=" : "", connector1 ? cstrFromOcppEveState(connector1->getStatus()) : "",
                connector2 ? "\nocpp_status_cId2=" : "", connector2 ? cstrFromOcppEveState(connector2->getStatus()) : "",
                context.getConnection().isConnected() ? "connected" : "unconnected",
                context.getConnection().getLastConnected() / 1000UL,
                context.getConnection().getLastRecv() / 1000UL,
                connector1 ? "\ncId1_hasTx=" : "", connector1 ? (connector1Tx ? "1" : "0") : "",
                connector1Tx ? "\ncId1_txActive=" : "", connector1Tx ? (connector1Tx->isActive() ? "1" : "0") : "",
                connector1Tx ? "\ncId1_txHasStarted=" : "", connector1Tx ? (connector1Tx->getStartSync().isRequested() ? "1" : "0") : "",
                connector1Tx ? "\ncId1_txHasStopped=" : "", connector1Tx ? (connector1Tx->getStopSync().isRequested() ? "1" : "0") : "",
                connector2 ? "\ncId2_hasTx=" : "", connector2 ? (connector2Tx ? "1" : "0") : "",
                connector2Tx ? "\ncId2_txActive=" : "", connector2Tx ? (connector2Tx->isActive() ? "1" : "0") : "",
                connector2Tx ? "\ncId2_txHasStarted=" : "", connector2Tx ? (connector2Tx->getStartSync().isRequested() ? "1" : "0") : "",
                connector2Tx ? "\ncId2_txHasStopped=" : "", connector2Tx ? (connector2Tx->getStopSync().isRequested() ? "1" : "0") : "",
                MO_ENABLE_CONNECTOR_LOCK,
                MO_ENABLE_FILE_INDEX,
                MO_ENABLE_V201
                );
        }

        if (filesystem) {

            if (ret >= 0 && (size_t)ret + diagPostambleLen < diagPostambleSize) {
                diagPostambleLen += (size_t)ret;
                ret = snprintf(diagPostamble + diagPostambleLen, diagPostambleSize - diagPostambleLen, "\n# Filesystem\n");
            }

            filesystem->ftw_root([this, &ret] (const char *fname) -> int {
                if (ret >= 0 && (size_t)ret + diagPostambleLen < diagPostambleSize) {
                    diagPostambleLen += (size_t)ret;
                    ret = snprintf(diagPostamble + diagPostambleLen, diagPostambleSize - diagPostambleLen, "%s\n", fname);
                }
                diagFileList.emplace_back(fname);
                return 0;
            });

            MO_DBG_DEBUG("discovered %zu files", diagFileList.size());
        }

        if (ret >= 0 && (size_t)ret + diagPostambleLen < diagPostambleSize) {
            diagPostambleLen += (size_t)ret;
        } else {
            char errMsg [64];
            auto errLen = snprintf(errMsg, sizeof(errMsg), "\n[Diagnostics cut]\n");
            size_t ellipseStart = std::min(diagPostambleSize - (size_t)errLen - 1, diagPostambleLen);
            auto ret2 = snprintf(diagPostamble + ellipseStart, diagPostambleSize - ellipseStart, "%s", errMsg);
            diagPostambleLen += (size_t)ret2;
        }

        this->ftpUpload = ftpClient->postFile(location,
            [this, diagnosticsReader, filesystem] (unsigned char *buf, size_t size) -> size_t {
                size_t written = 0;
                if (written < size && diagPreambleTransferred < diagPreambleLen) {
                    size_t writeLen = std::min(size - written, diagPreambleLen - diagPreambleTransferred);
                    memcpy(buf + written, diagPreamble + diagPreambleTransferred, writeLen);
                    diagPreambleTransferred += writeLen;
                    written += writeLen;
                }

                while (written < size && diagReaderHasData && diagnosticsReader) {
                    size_t writeLen = diagnosticsReader((char*)buf + written, size - written);
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
                    auto ret = snprintf(fpath, sizeof(fpath), "%s%s", MO_FILENAME_PREFIX, diagFileList.back().c_str());
                    if (ret < 0 || (size_t)ret >= sizeof(fpath)) {
                        MO_DBG_ERR("fn error: %i", ret);
                        diagFileList.pop_back();
                        continue;
                    }

                    if (auto file = filesystem->open(fpath, "r")) {

                        if (diagFilesBackTransferred == 0) {
                            char fileHeading [30 + MO_MAX_PATH_SIZE];
                            auto writeLen = snprintf(fileHeading, sizeof(fileHeading), "\n\n# File %s:\n", diagFileList.back().c_str());
                            if (writeLen < 0 || (size_t)writeLen >= sizeof(fileHeading)) {
                                MO_DBG_ERR("fn error: %i", ret);
                                diagFileList.pop_back();
                                continue;
                            }
                            if (writeLen + written > size || //heading doesn't fit anymore, return with a bit unused buffer space and print heading the next time
                                    writeLen + written == size) { //filling the buffer up exactly would mean that no file payload is written and this head gets printed again
                                
                                MO_DBG_DEBUG("upload diag chunk (%zuB)", written);
                                return written;
                            }

                            memcpy(buf + written, fileHeading, (size_t)writeLen);
                            written += (size_t)writeLen;
                        }

                        file->seek(diagFilesBackTransferred);
                        size_t writeLen = file->read((char*)buf + written, size - written);

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
            [this, onClose] (MO_FtpCloseReason reason) -> void {
                if (reason == MO_FtpCloseReason_Success) {
                    MO_DBG_INFO("FTP upload success");
                    this->ftpUploadStatus = UploadStatus::Uploaded;
                } else {
                    MO_DBG_INFO("FTP upload failure (%i)", reason);
                    this->ftpUploadStatus = UploadStatus::UploadFailed;
                }

                MO_FREE(diagPreamble);
                MO_FREE(diagPostamble);
                diagFileList.clear();

                if (onClose) {
                    onClose();
                }
            });

        if (this->ftpUpload) {
            this->ftpUploadStatus = UploadStatus::NotUploaded;
            return true;
        } else {
            this->ftpUploadStatus = UploadStatus::UploadFailed;
            return false;
        }
    };

    this->uploadStatusInput = [this] () {
        return this->ftpUploadStatus;
    };
}

void DiagnosticsService::setFtpServerCert(const char *cert) {
    this->ftpServerCert = cert;
}

#if !defined(MO_CUSTOM_DIAGNOSTICS)

#if MO_PLATFORM == MO_PLATFORM_ARDUINO && defined(ESP32) && MO_ENABLE_MBEDTLS

#include "esp_heap_caps.h"
#include <LittleFS.h>

bool g_diagsSent = false;

std::unique_ptr<DiagnosticsService> MicroOcpp::makeDefaultDiagnosticsService(Context& context, std::shared_ptr<FilesystemAdapter> filesystem) {
    std::unique_ptr<DiagnosticsService> diagService = std::unique_ptr<DiagnosticsService>(new DiagnosticsService(context));

    diagService->setDiagnosticsReader(
        [] (char *buf, size_t size) -> size_t {
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
        }, [] () {
            g_diagsSent = false;
        },
        filesystem);

    return diagService;
}

#elif MO_ENABLE_MBEDTLS

std::unique_ptr<DiagnosticsService> MicroOcpp::makeDefaultDiagnosticsService(Context& context, std::shared_ptr<FilesystemAdapter> filesystem) {
    std::unique_ptr<DiagnosticsService> diagService = std::unique_ptr<DiagnosticsService>(new DiagnosticsService(context));

    diagService->setDiagnosticsReader(nullptr, nullptr, filesystem); //report the built-in MO defaults

    return diagService;
}

#endif //MO_PLATFORM
#endif //!defined(MO_CUSTOM_DIAGNOSTICS)
