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
    uint32_t (*ticksCb)() = nullptr;
    uint32_t (*rngCb)() = nullptr;

#if MO_USE_FILEAPI != MO_CUSTOM_FS
    MO_FilesystemConfig filesystemConfig;
    bool filesystemConfigDefined = false;
    bool isFilesystemOwner = false;
#endif //MO_USE_FILEAPI != MO_CUSTOM_FS
    MO_FilesystemAdapter *filesystem = nullptr;
    
#if MO_ENABLE_MOCK_SERVER
    bool useMockConnection = false;
#endif //MO_ENABLE_MOCK_SERVER
#if MO_WS_USE != MO_WS_CUSTOM
    MO_ConnectionConfig connectionConfig;
    bool connectionConfigDefined = false;
    bool useDefaultConnection = false;
#endif //MO_WS_USE != MO_WS_CUSTOM
    MO_Connection *connection = nullptr;

#if MO_ENABLE_MBEDTLS
    MO_FTPConfig ftpConfig;
    bool ftpConfigDefined = false;
    bool isFtpClientOwner = false;
#endif //MO_ENABLE_MBEDTLS
    FtpClient *ftpClient = nullptr;

    CertificateStore *certStore = nullptr;
    bool isCertStoreOwner = false;

    Clock clock {*this};
    MessageService msgService {*this};

#if MO_ENABLE_V16
    v16::Model modelV16 {*this};
#endif
#if MO_ENABLE_V201
    v201::Model modelV201 {*this};
#endif
    int ocppVersion = MO_ENABLE_V16 ? MO_OCPP_V16 : MO_ENABLE_V201 ? MO_OCPP_V201 : -1;

public:
    Context();
    ~Context();

    void setDebugCb(void (*debugCb)(const char *msg));
    void setDebugCb2(void (*debugCb2)(int lvl, const char *fn, int line, const char *msg));

    void setTicksCb(uint32_t (*ticksCb)());
    uint32_t getTicksMs();

    void setRngCb(uint32_t (*rngCb)());
    uint32_t (*getRngCb())();

#if MO_USE_FILEAPI != MO_CUSTOM_FS
    bool setFilesystemConfig(MO_FilesystemConfig filesystemConfig);
#endif //MO_USE_FILEAPI != MO_CUSTOM_FS
    void setFilesystem(MO_FilesystemAdapter *filesystem);
    MO_FilesystemAdapter *getFilesystem();

#if MO_WS_USE != MO_WS_CUSTOM
    bool setConnectionConfig(MO_ConnectionConfig connectionConfig);
#endif //MO_WS_USE != MO_WS_CUSTOM
    void setConnection(MO_Connection *connection);
    MO_Connection *getConnection();

#if MO_ENABLE_MOCK_SERVER
    void useMockServer();
#endif //MO_ENABLE_MOCK_SERVER

#if MO_ENABLE_MBEDTLS
    void setFtpConfig(MO_FTPConfig ftpConfig);
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
    v16::Model& getModel16();
#endif

#if MO_ENABLE_V201
    v201::Model& getModel201();
#endif

#if MO_ENABLE_V16 || MO_ENABLE_V201
    ModelCommon& getModelCommon(); //subset of Model16 / Model201 with modules which are shared between v16 and v201 implementation
#endif //MO_ENABLE_V16 || MO_ENABLE_V201

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
