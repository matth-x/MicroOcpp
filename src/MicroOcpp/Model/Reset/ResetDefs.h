// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_RESETDEFS_H
#define MO_RESETDEFS_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

typedef enum MO_ResetType {
    MO_ResetType_Immediate,
    MO_ResetType_OnIdle
}   MO_ResetType;

typedef enum MO_ResetStatus {
    MO_ResetStatus_Accepted,
    MO_ResetStatus_Rejected,
    MO_ResetStatus_Scheduled
}   MO_ResetStatus;

#ifdef __cplusplus
} //extern "C"
#endif //__cplusplus

#endif //MO_ENABLE_V201
#endif
