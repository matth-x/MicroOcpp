// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_CONTEXT_H
#define MO_CONTEXT_H

#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/OperationRegistry.h>
#include <MicroOcpp/Core/RequestQueue.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Version.h>

#ifdef __cplusplus

namespace MicroOcpp {

class FilesystemAdapter;
class Connection;
class FtpClient;

class Context : public MemoryManaged {
private:
    void (*debugCb)(const char *msg) = nullptr;
    void (*debugCb2)(int lvl, const char *fn, int line, const char *msg) = nullptr;

    unsigned long (*ticksCb)() = nullptr;
    
    FilesystemAdapter *filesystem = nullptr;
    Connection *connection = nullptr;
    FtpClient *ftpClient = nullptr;

    Model model;
    OperationRegistry operationRegistry;
    RequestQueue reqQueue;

    bool isFilesystemOwner = false;
    bool isConnectionOwner = false;
    bool isFtpClientOwner = false;

public:
    Context();
    ~Context();

    void setDebugCb(void (*debugCb)(const char *msg));
    void setDebugCb2(void (*debugCb2)(int lvl, const char *fn, int line, const char *msg));

    void setTicksCb(unsigned long (*ticksCb)());

    void setFilesystemAdapter(FilesystemAdapter *filesystem);
    FilesystemAdapter *getFilesystemAdapter();

    void setConnection(Connection *connection);
    Connection *getConnection();

    void setFtpClient(FtpClient *ftpClient);
    FtpClient *getFtpClient();

    Model& getModel();

    bool setup(int ocppVersion);

    void loop();

    OperationRegistry& getOperationRegistry();

    RequestQueue& getRequestQueue();
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

bool mo_context_setup(mo_context *ctx, int ocpp_version);

void mo_context_loop(mo_context *ctx);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif
