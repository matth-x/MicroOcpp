// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Diagnostics/Diagnostics.h>

#include <string.h>

#include <MicroOcpp/Debug.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_DIAGNOSTICS

const char *mo_serializeGetLogStatus(MO_GetLogStatus status) {
    const char *statusCstr = "";
    switch (status) {
        case MO_GetLogStatus_Accepted:
            statusCstr = "Accepted";
            break;
        case MO_GetLogStatus_Rejected:
            statusCstr = "Rejected";
            break;
        case MO_GetLogStatus_AcceptedCanceled:
            statusCstr = "AcceptedCanceled";
            break;
        case MO_GetLogStatus_UNDEFINED:
            MO_DBG_ERR("serialize undefined");
            break;
    }
    return statusCstr;
}

const char *mo_serializeUploadLogStatus(MO_UploadLogStatus status) {
    const char *statusCstr = "";
    switch (status) {
        case MO_UploadLogStatus_BadMessage:
            statusCstr = "BadMessage";
            break;
        case MO_UploadLogStatus_Idle:
            statusCstr = "Idle";
            break;
        case MO_UploadLogStatus_NotSupportedOperation:
            statusCstr = "NotSupportedOperation";
            break;
        case MO_UploadLogStatus_PermissionDenied:
            statusCstr = "PermissionDenied";
            break;
        case MO_UploadLogStatus_Uploaded:
            statusCstr = "Uploaded";
            break;
        case MO_UploadLogStatus_UploadFailure:
            statusCstr = "UploadFailure";
            break;
        case MO_UploadLogStatus_Uploading:
            statusCstr = "Uploading";
            break;
        case MO_UploadLogStatus_AcceptedCanceled:
            statusCstr = "AcceptedCanceled";
            break;
    }
    return statusCstr;
}

MO_LogType mo_deserializeLogType(const char *v) {
    if (!v) {
        return MO_LogType_UNDEFINED;
    }

    MO_LogType res = MO_LogType_UNDEFINED;

    if (!strcmp(v, "DiagnosticsLog")) {
        res = MO_LogType_DiagnosticsLog;
    } else if (!strcmp(v, "SecurityLog")) {
        res = MO_LogType_SecurityLog;
    }

    return res;
}

#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_DIAGNOSTICS

#if MO_ENABLE_V16 && MO_ENABLE_DIAGNOSTICS
namespace MicroOcpp {
namespace v16 {

const char *serializeDiagnosticsStatus(DiagnosticsStatus status) {
    const char *statusCstr = "";
    switch (status) {
        case DiagnosticsStatus::Idle:
            statusCstr = "Idle";
            break;
        case DiagnosticsStatus::Uploaded:
            statusCstr = "Uploaded";
            break;
        case DiagnosticsStatus::UploadFailed:
            statusCstr = "UploadFailed";
            break;
        case DiagnosticsStatus::Uploading:
            statusCstr = "Uploading";
            break;
    }
    return statusCstr;
}

} //namespace v16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16 && MO_ENABLE_DIAGNOSTICS
