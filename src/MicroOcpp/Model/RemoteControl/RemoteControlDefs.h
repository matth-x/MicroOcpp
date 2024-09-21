// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_UNLOCKCONNECTOR_H
#define MO_UNLOCKCONNECTOR_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <stdint.h>

#include <MicroOcpp/Model/ConnectorBase/UnlockConnectorResult.h>

typedef enum {
    RequestStartStopStatus_Accepted,
    RequestStartStopStatus_Rejected
}   RequestStartStopStatus;

#if MO_ENABLE_CONNECTOR_LOCK

typedef enum {
    UnlockStatus_Unlocked,
    UnlockStatus_UnlockFailed,
    UnlockStatus_OngoingAuthorizedTransaction,
    UnlockStatus_UnknownConnector,
    UnlockStatus_PENDING // unlock action not finished yet, result still unknown (MO will check again later)
}   UnlockStatus;

#endif // MO_ENABLE_CONNECTOR_LOCK

#endif // MO_ENABLE_V201
#endif // MO_UNLOCKCONNECTOR_H
