// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_UNLOCKCONNECTOR_H
#define MO_UNLOCKCONNECTOR_H

#include <stdint.h>
#include <MicroOcpp/Version.h>

// Connector-lock related behavior (i.e. if UnlockConnectorOnEVSideDisconnect is RW; enable HW binding for UnlockConnector)
#ifndef MO_ENABLE_CONNECTOR_LOCK
#define MO_ENABLE_CONNECTOR_LOCK 1
#endif

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_CONNECTOR_LOCK

#ifndef MO_UNLOCK_TIMEOUT
#define MO_UNLOCK_TIMEOUT 10 // if Result is Pending, wait at most this period (in ms) until sending UnlockFailed
#endif

typedef enum {
    MO_UnlockConnectorResult_UnlockFailed,
    MO_UnlockConnectorResult_Unlocked,
    MO_UnlockConnectorResult_Pending // unlock action not finished yet, result still unknown (MO will check again later)
} MO_UnlockConnectorResult;

#ifdef __cplusplus
}
#endif // __cplusplus

namespace MicroOcpp {

enum class TriggerMessageStatus : uint8_t {
    ERR_INTERNAL,
    Accepted,
    Rejected,
    NotImplemented,
};

} //namespace MicroOcpp

#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_CONNECTOR_LOCK

#if MO_ENABLE_V16
#ifdef __cplusplus

namespace MicroOcpp {
namespace Ocpp16 {

enum class RemoteStartStopStatus : uint8_t {
    ERR_INTERNAL,
    Accepted,
    Rejected
};

enum class UnlockStatus : uint8_t {
    Unlocked,
    UnlockFailed,
    NotSupported,
    PENDING //MO-internal: unlock action not finished yet, result still unknown. Check later
};

} //namespace Ocpp16
} //namespace MicroOcpp

#endif //__cplusplus
#endif //MO_ENABLE_V16

#if MO_ENABLE_V201
#ifdef __cplusplus

namespace MicroOcpp {
namespace Ocpp201 {

enum class RequestStartStopStatus : uint8_t {
    Accepted,
    Rejected
};

enum class UnlockStatus : uint8_t {
    Unlocked,
    UnlockFailed,
    OngoingAuthorizedTransaction,
    UnknownConnector,
    PENDING //MO-internal: unlock action not finished yet, result still unknown. Check later
};

} //namespace Ocpp201
} //namespace MicroOcpp
#endif //__cplusplus
#endif //MO_ENABLE_V201

#endif //MO_UNLOCKCONNECTOR_H
