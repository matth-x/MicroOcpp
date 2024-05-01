// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_TRANSACTIONDEFS_H
#define MO_TRANSACTIONDEFS_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#define MO_TXID_LEN_MAX 36

typedef enum RequestStartStopStatus {
    RequestStartStopStatus_Accepted,
    RequestStartStopStatus_Rejected
}   RequestStartStopStatus;

#endif //MO_ENABLE_V201
#endif
