// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_UNLOCKCONNECTORRESULT_H
#define MO_UNLOCKCONNECTORRESULT_H

#include <stdint.h>

// Connector-lock related behavior (i.e. if UnlockConnectorOnEVSideDisconnect is RW; enable HW binding for UnlockConnector)
#ifndef MO_ENABLE_CONNECTOR_LOCK
#define MO_ENABLE_CONNECTOR_LOCK 0
#endif

#if MO_ENABLE_CONNECTOR_LOCK

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#ifndef MO_UNLOCK_TIMEOUT
#define MO_UNLOCK_TIMEOUT 10000 // if Result is Pending, wait at most this period (in ms) until sending UnlockFailed
#endif

typedef enum {
    UnlockConnectorResult_UnlockFailed,
    UnlockConnectorResult_Unlocked,
    UnlockConnectorResult_Pending // unlock action not finished yet, result still unknown (MO will check again later)
} UnlockConnectorResult;

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // MO_ENABLE_CONNECTOR_LOCK
#endif // MO_UNLOCKCONNECTORRESULT_H
