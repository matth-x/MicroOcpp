// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_RESETDEFS_H
#define MO_RESETDEFS_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

typedef enum ResetType {
    ResetType_Immediate,
    ResetType_OnIdle
}   ResetType;

typedef enum ResetStatus {
    ResetStatus_Accepted,
    ResetStatus_Rejected,
    ResetStatus_Scheduled
}   ResetStatus;

#endif //MO_ENABLE_V201
#endif 
