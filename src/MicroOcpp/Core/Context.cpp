// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Ftp.h>

#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

Context::Context() : MemoryManaged("Context") {

}

Context::~Context() {
    if (filesystem && isFilesystemOwner) {
        delete filesystem;
        filesystem = nullptr;
        isFilesystemOwner = false;
    }

    if (connection && isConnectionOwner) {
        delete connection;
        connection = nullptr;
        isConnectionOwner = false;
    }

    if (ftpClient && isFtpClientOwner) {
        delete ftpClient;
        ftpClient = nullptr;
        isFtpClientOwner = false;
    }
}

void Context::setDebugCb(void (*debugCb)(const char *msg)) {
    this->debugCb = debugCb;
}

void Context::setDebugCb2(void (*debugCb2)(int lvl, const char *fn, int line, const char *msg)) {
    this->debugCb2 = debugCb2;
}

void Context::setTicksCb(unsigned long (*ticksCb)()) {
    this->ticksCb = ticksCb;
}

void Context::setFilesystemAdapter(FilesystemAdapter *filesystem) {
    if (this->filesystem && isFilesystemOwner) {
        delete this->filesystem;
        this->filesystem = nullptr;
        isFilesystemOwner = false;
    }
    this->filesystem = filesystem;
}

FilesystemAdapter *Context::getFilesystemAdapter() {
    return filesystem;
}

void Context::setConnection(Connection *connection) {
    if (connection && isConnectionOwner) {
        delete connection;
        connection = nullptr;
        isConnectionOwner = false;
    }
    this->connection = connection;
}

Connection *Context::getConnection() {
    return connection;
}

void Context::setFtpClient(FtpClient *ftpClient) {
    if (ftpClient && isFtpClientOwner) {
        delete ftpClient;
        ftpClient = nullptr;
        isFtpClientOwner = false;
    }
    ftpClient = ftpClient;
}

FtpClient *Context::getFtpClient() {
    return ftpClient;
}

bool Context::setup(int protocolVersion) {

}

    void Context::loop();

    Model& getModel();

    OperationRegistry& getOperationRegistry();

    RequestQueue& getRequestQueue();

    const ProtocolVersion& getVersion();
};

} //end namespace MicroOcpp

#endif // __cplusplus

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

struct mo_context;
typedef struct mo_context mo_context;

mo_context *mo_context_make();

void mo_context_free(mo_context *ctx);

void mo_context_dbg_cb_set(mo_context *ctx, void (*debug_cb)(const char *msg));

void mo_context_dbg_cb2_set(mo_context *ctx, void (*debug_cb)(int lvl, const char *fn, int line, const char *msg));

void mo_context_ticks_cb_set(unsigned long (*ticksCb)());

bool mo_context_setup(mo_context *ctx, int protocol_version);

void mo_context_loop(mo_context *ctx);

void Context::loop() {
    connection.loop();
    reqQueue.loop();
    model.loop();
}

Model& Context::getModel() {
    return model;
}

OperationRegistry& Context::getOperationRegistry() {
    return operationRegistry;
}

const ProtocolVersion& Context::getVersion() {
    return model.getVersion();
}

Connection& Context::getConnection() {
    return connection;
}

RequestQueue& Context::getRequestQueue() {
    return reqQueue;
}

void Context::setFtpClient(std::unique_ptr<FtpClient> ftpClient) {
    this->ftpClient = std::move(ftpClient);
}

FtpClient *Context::getFtpClient() {
    return ftpClient.get();
}
