// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CHARGEPOINTSTATUS_H
#define MO_CHARGEPOINTSTATUS_H

#include <MicroOcpp/Version.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ChargePointStatus {
    ChargePointStatus_UNDEFINED, //internal use only - no OCPP standard value 
    ChargePointStatus_Available,
    ChargePointStatus_Preparing,
    ChargePointStatus_Charging,
    ChargePointStatus_SuspendedEVSE,
    ChargePointStatus_SuspendedEV,
    ChargePointStatus_Finishing,
    ChargePointStatus_Reserved,
    ChargePointStatus_Unavailable,
    ChargePointStatus_Faulted

#if MO_ENABLE_V201
    ,ChargePointStatus_Occupied
#endif

}   ChargePointStatus;

#ifdef __cplusplus
}
#endif

#endif
