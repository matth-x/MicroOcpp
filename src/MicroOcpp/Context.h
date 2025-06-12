// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_CONTEXT_H
#define MO_CONTEXT_H

#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/Ftp.h>
#include <MicroOcpp/Core/FtpMbedTLS.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Core/MessageService.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Platform.h>
#include <MicroOcpp/Version.h>

#ifdef __cplusplus
extern "C" {
#endif

//Anonymous handle to pass Context object around API functions
struct MO_Context;
typedef struct MO_Context MO_Context;

#ifdef __cplusplus
} //extern "C" {

namespace MicroOcpp {

class CertificateStore;

class Context : public MemoryManaged {
private:
    unsigned long (*ticksCb)() = nullptr;
    uint32_t (*rngCb)() = nullptr;

#if MO_USE_FILEAPI != MO_CUSTOM_FS
    MO_FilesystemConfig filesystemConfig;
    bool isFilesystemOwner = false;
#endif //MO_USE_FILEAPI != MO_CUSTOM_FS
    MO_FilesystemAdapter *filesystem = nullptr;

#if MO_WS_USE != MO_WS_CUSTOM
    MO_ConnectionConfig connectionConfig;
    bool isConnectionOwner = false;
#endif //MO_WS_USE != MO_WS_CUSTOM
    Connection *connection = nullptr;

#if MO_ENABLE_MBEDTLS
    MO_FTPConfig ftpConfig;
    bool isFtpClientOwner = false;
#endif //MO_ENABLE_MBEDTLS
    FtpClient *ftpClient = nullptr;

    CertificateStore *certStore = nullptr;
    bool isCertStoreOwner = false;

    Clock clock {*this};
    MessageService msgService {*this};

#if MO_ENABLE_V16
    Ocpp16::Model modelV16 {*this};
#endif
#if MO_ENABLE_V201
    Ocpp201::Model modelV201 {*this};
#endif
    int ocppVersion = MO_ENABLE_V16 ? MO_OCPP_V16 : MO_ENABLE_V201 ? MO_OCPP_V201 : -1;

public:
    Context();
    ~Context();

    void setDebugCb(void (*debugCb)(const char *msg));
    void setDebugCb2(void (*debugCb2)(int lvl, const char *fn, int line, const char *msg));

    void setTicksCb(unsigned long (*ticksCb)());
    unsigned long getTicksMs();

    void setRngCb(uint32_t (*rngCb)());
    uint32_t (*getRngCb())();

#if MO_USE_FILEAPI != MO_CUSTOM_FS
    void setDefaultFilesystemConfig(MO_FilesystemConfig filesystemConfig);
#endif //MO_USE_FILEAPI != MO_CUSTOM_FS
    void setFilesystem(MO_FilesystemAdapter *filesystem);
    MO_FilesystemAdapter *getFilesystem();

#if MO_WS_USE != MO_WS_CUSTOM
    void setDefaultConnectionConfig(MO_ConnectionConfig connectionConfig);
#endif //MO_WS_USE != MO_WS_CUSTOM
    void setConnection(Connection *connection);
    Connection *getConnection();

#if MO_ENABLE_MBEDTLS
    void setDefaultFtpConfig(MO_FTPConfig ftpConfig);
#endif //MO_ENABLE_MBEDTLS
    void setFtpClient(FtpClient *ftpClient);
    FtpClient *getFtpClient();

    void setCertificateStore(CertificateStore *certStore);
    CertificateStore *getCertificateStore();

    Clock& getClock();
    MessageService& getMessageService();

    bool setOcppVersion(int ocppVersion);
    int getOcppVersion();

#if MO_ENABLE_V16
    Ocpp16::Model& getModel16();
#endif

#if MO_ENABLE_V201
    Ocpp201::Model& getModel201();
#endif

    // eliminate OCPP version specifiers if charger supports just one version anyway
#if MO_ENABLE_V16 && !MO_ENABLE_V201
    Model& getModel();
#elif !MO_ENABLE_V16 && MO_ENABLE_V201
    Model& getModel();
#endif

    bool setup();

    void loop();
};

} //namespace MicroOcpp

#endif // __cplusplus
#endif
