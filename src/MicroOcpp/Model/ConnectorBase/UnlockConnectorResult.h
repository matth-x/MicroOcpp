// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_UNLOCKCONNECTORRESULT_H
#define MO_UNLOCKCONNECTORRESULT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#ifndef MO_UNLOCK_TIMEOUT
#define MO_UNLOCK_TIMEOUT 10000 // if Result is Pending, wait at most this period (in ms) until sending UnlockFailed
#endif

typedef enum {
    UnlockFailed,
    Unlocked,
    Pending // unlock action not finished yet, result still unknown (MO will check again later)
} UnlockConnectorResult;

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // MO_UNLOCKCONNECTORRESULT_H
