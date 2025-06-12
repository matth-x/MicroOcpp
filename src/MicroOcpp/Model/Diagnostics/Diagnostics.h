// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_DIAGNOSTICS_H
#define MO_DIAGNOSTICS_H

#include <MicroOcpp/Version.h>

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#ifndef MO_GETLOG_FNAME_SIZE
#define MO_GETLOG_FNAME_SIZE 30
#endif

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_DIAGNOSTICS

typedef enum {
    MO_GetLogStatus_UNDEFINED, //MO-internal
    MO_GetLogStatus_Accepted,
    MO_GetLogStatus_Rejected,
    MO_GetLogStatus_AcceptedCanceled,
}   MO_GetLogStatus;
const char *mo_serializeGetLogStatus(MO_GetLogStatus status);

// UploadLogStatusEnumType (6.17 in 1.6 Security whitepaper, 3.87 in 2.0.1 spec)
typedef enum {
    MO_UploadLogStatus_BadMessage,
    MO_UploadLogStatus_Idle,
    MO_UploadLogStatus_NotSupportedOperation,
    MO_UploadLogStatus_PermissionDenied,
    MO_UploadLogStatus_Uploaded,
    MO_UploadLogStatus_UploadFailure,
    MO_UploadLogStatus_Uploading,
    MO_UploadLogStatus_AcceptedCanceled,

}   MO_UploadLogStatus;
const char *mo_serializeUploadLogStatus(MO_UploadLogStatus status);

typedef enum {
    MO_UploadStatus_NotUploaded,
    MO_UploadStatus_Uploaded,
    MO_UploadStatus_UploadFailed
}   MO_UploadStatus;

typedef enum {
    MO_LogType_UNDEFINED,
    MO_LogType_DiagnosticsLog,
    MO_LogType_SecurityLog,
}   MO_LogType;
MO_LogType mo_deserializeLogType(const char *v);

#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_DIAGNOSTICS

#ifdef __cplusplus
} //extern "C"

#if MO_ENABLE_V16 && MO_ENABLE_DIAGNOSTICS

namespace MicroOcpp {
namespace Ocpp16 {

enum class DiagnosticsStatus {
    Idle,
    Uploaded,
    UploadFailed,
    Uploading
};
const char *serializeDiagnosticsStatus(DiagnosticsStatus status);

} //namespace Ocpp16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16 && MO_ENABLE_DIAGNOSTICS

#endif //__cplusplus
#endif
