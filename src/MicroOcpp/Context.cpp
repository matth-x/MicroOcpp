// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Ftp.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Certificates/Certificate.h>

#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

Context::Context() : MemoryManaged("Context") {
#if MO_USE_FILEAPI != MO_CUSTOM_FS
    memset(&filesystemConfig, 0, sizeof(filesystemConfig));
    filesystemConfig.opt = MO_FS_OPT_USE_MOUNT;
    filesystemConfig.path_prefix = MO_FILENAME_PREFIX;
#endif //MO_USE_FILEAPI != MO_CUSTOM_FS

#if MO_WS_USE != MO_WS_CUSTOM
    memset(&connectionConfig, 0, sizeof(connectionConfig));
#endif //MO_WS_USE != MO_WS_CUSTOM

#if MO_ENABLE_MBEDTLS
    memset(&ftpConfig, 0, sizeof(ftpConfig));
#endif //MO_ENABLE_MBEDTLS
}

Context::~Context() {
    // reset resources (frees resources if owning them)
    setFilesystem(nullptr);
    setConnection(nullptr);
    setFtpClient(nullptr);
    setCertificateStore(nullptr);
}

void Context::setDebugCb(void (*debugCb)(const char *msg)) {
    debug.setDebugCb(debugCb); // `debug` globally declared in Debug.h
}

void Context::setDebugCb2(void (*debugCb2)(int lvl, const char *fn, int line, const char *msg)) {
    debug.setDebugCb2(debugCb2); // `debug` globally declared in Debug.h
}

void Context::setTicksCb(unsigned long (*ticksCb)()) {
    this->ticksCb = ticksCb;
}

unsigned long Context::getTicksMs() {
    if (this->ticksCb) {
        return this->ticksCb();
    } else {
        MO_DBG_ERR("ticksCb not set yet");
        return 0;
    }
}

void Context::setRngCb(uint32_t (*rngCb)()) {
    this->rngCb = rngCb;
}

uint32_t (*Context::getRngCb())() {
    return rngCb;
}

#if MO_USE_FILEAPI != MO_CUSTOM_FS
void Context::setDefaultFilesystemConfig(MO_FilesystemConfig filesystemConfig) {
    this->filesystemConfig = filesystemConfig;
}
#endif //MO_USE_FILEAPI != MO_CUSTOM_FS

void Context::setFilesystem(MO_FilesystemAdapter *filesystem) {
#if MO_USE_FILEAPI != MO_CUSTOM_FS
    if (this->filesystem && isFilesystemOwner) {
        mo_freeDefaultFilesystemAdapter(this->filesystem);
        this->filesystem = nullptr;
    }
    isFilesystemOwner = false;
#endif //MO_USE_FILEAPI != MO_CUSTOM_FS
    this->filesystem = filesystem;
}

MO_FilesystemAdapter *Context::getFilesystem() {
    return filesystem;
}

#if MO_WS_USE != MO_WS_CUSTOM
void Context::setDefaultConnectionConfig(MO_ConnectionConfig connectionConfig) {
    this->connectionConfig = connectionConfig;
}
#endif //MO_WS_USE != MO_WS_CUSTOM

void Context::setConnection(Connection *connection) {
    if (this->connection) {
        this->connection->setContext(nullptr);
    }
#if MO_WS_USE != MO_WS_CUSTOM
    if (this->connection && isConnectionOwner) {
        freeDefaultConnection(this->connection);
        this->connection = nullptr;
    }
    isConnectionOwner = false;
#endif //MO_WS_USE != MO_WS_CUSTOM
    this->connection = connection;
    if (this->connection) {
        this->connection->setContext(this);
    }
}

Connection *Context::getConnection() {
    return connection;
}

void Context::setFtpClient(FtpClient *ftpClient) {
    if (this->ftpClient && isFtpClientOwner) {
        delete this->ftpClient;
        this->ftpClient = nullptr;
    }
    isFtpClientOwner = false;
    this->ftpClient = ftpClient;
}

FtpClient *Context::getFtpClient() {
    return ftpClient;
}

void Context::setCertificateStore(CertificateStore *certStore) {
    if (this->certStore && isCertStoreOwner) {
        delete this->certStore;
        this->certStore = nullptr;
    }
    isCertStoreOwner = false;
    this->certStore = certStore;
}

CertificateStore *Context::getCertificateStore() {
    return certStore;
}

Clock& Context::getClock() {
    return clock;
}

MessageService& Context::getMessageService() {
    return msgService;
}

bool Context::setOcppVersion(int ocppVersion) {
    if (ocppVersion == MO_OCPP_V16 && MO_ENABLE_V16) {
        this->ocppVersion = ocppVersion;
        return true;
    } else if (ocppVersion == MO_OCPP_V201 && MO_ENABLE_V201) {
        this->ocppVersion = ocppVersion;
        return true;
    } else {
        MO_DBG_ERR("unsupported OCPP version");
        return false;
    }
}

int Context::getOcppVersion() {
    return ocppVersion;
}

#if MO_ENABLE_V16
Ocpp16::Model& Context::getModel16() {
    return modelV16;
}
#endif

#if MO_ENABLE_V201
Ocpp201::Model& Context::getModel201() {
    return modelV201;
}
#endif

#if MO_ENABLE_V16 && !MO_ENABLE_V201
Model& Context::getModel() {
    return getModel16();
}
#elif !MO_ENABLE_V16 && MO_ENABLE_V201
Model& Context::getModel() {
    return getModel201();
}
#endif

bool Context::setup() {

    if (!debug.setup()) {
        MO_DBG_ERR("must set debugCb before"); //message won't show up on console
        return false;
    }

    if (!ticksCb) {
        ticksCb = getDefaultTickCb();
        
        if (!ticksCb) {
            MO_DBG_ERR("must set TicksCb connection before setup. See the examples in this repository");
            return false;
        }
    }

    if (!rngCb) {
        rngCb = getDefaultRngCb();
        if (!rngCb) {
            MO_DBG_ERR("random number generator cannot be found");
            return false;
        }
    }

#if MO_USE_FILEAPI != MO_CUSTOM_FS
    if (!filesystem && filesystemConfig.opt != MO_FS_OPT_DISABLE) {
        // init default FS implementaiton
        filesystem = mo_makeDefaultFilesystemAdapter(filesystemConfig);
        if (!filesystem) {
            MO_DBG_ERR("OOM");
            return false;
        }
        isFilesystemOwner = true;
    }
#endif //MO_USE_FILEAPI != MO_CUSTOM_FS

    if (!filesystem) {
        MO_DBG_DEBUG("initialize MO without filesystem access");
    }

#if MO_WS_USE != MO_WS_CUSTOM
    if (!connection) {
        // init default Connection implementation
        connection = static_cast<Connection*>(makeDefaultConnection(connectionConfig));
        if (!connection) {
            MO_DBG_ERR("OOM");
            return false;
        }
        isConnectionOwner = true;
    }
#endif //MO_WS_USE != MO_WS_CUSTOM

    if (!connection) {
        MO_DBG_ERR("must set WebSocket connection before setup. See the examples in this repository");
        return false;
    }

#if MO_ENABLE_MBEDTLS
    if (!ftpClient) {
        ftpClient = makeFtpClientMbedTLS(ftpConfig).release();
        if (!ftpClient) {
            MO_DBG_ERR("");
            return false;
        }
        isFtpClientOwner = true;
    }
#endif //MO_ENABLE_MBEDTLS

    if (!ftpClient) {
        MO_DBG_DEBUG("initialize MO without FTP client");
    }

    if (!certStore) {
        #if MO_ENABLE_CERT_MGMT && MO_ENABLE_CERT_STORE_MBEDTLS
        {
            certStore = makeCertificateStoreMbedTLS(filesystem);
            if (!certStore) {
                return false;
            }
            isCertStoreOwner = true;
        }
        #endif //MO_ENABLE_CERT_MGMT && MO_ENABLE_CERT_STORE_MBEDTLS

        if (MO_ENABLE_CERT_MGMT && !certStore) {
            MO_DBG_DEBUG("initialize MO without certificate store");
        }
    }

    if (!clock.setup()) {
        return false;
    }

    if (!msgService.setup()) {
        return false;
    }

    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {
        modelV16.setup();
    }
    #endif
    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {
        modelV201.setup();
    }
    #endif

    return true;
}

void Context::loop() {
    if (connection) {
        connection->loop();
    }
    clock.loop();
    msgService.loop();

    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {
        modelV16.loop();
    }
    #endif
    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {
        modelV201.loop();
    }
    #endif
}
