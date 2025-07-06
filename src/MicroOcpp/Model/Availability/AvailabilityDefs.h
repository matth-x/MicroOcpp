// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_AVAILABILITYDEFS_H
#define MO_AVAILABILITYDEFS_H

#include <stdint.h>

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool isError; //if any error information is set
    bool isFaulted; //if this is a severe error and the EVSE should go into the faulted state
    uint8_t severity; //severity: don't send less severe errors during highly severe error condition
    const char *errorCode; //see ChargePointErrorCode (p. 76/77) for possible values
    const char *info; //Additional free format information related to the error
    const char *vendorId; //vendor-specific implementation identifier
    const char *vendorErrorCode; //vendor-specific error code
} MO_ErrorData;

void mo_errorData_init(MO_ErrorData *errorData);
void mo_errorData_setErrorCode(MO_ErrorData *errorData, const char *errorCode);

typedef struct {
    MO_ErrorData (*getErrorData)(unsigned int evseId, void *userData);
    void *userData;
} MO_ErrorDataInput;

void mo_errorDataInput_init(MO_ErrorDataInput *errorDataInput);

#ifdef __cplusplus
} //extern "C"
#endif

#endif //MO_ENABLE_V16

#if MO_ENABLE_V16 || MO_ENABLE_V201
#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

typedef enum MO_ChargePointStatus {
    MO_ChargePointStatus_UNDEFINED, //internal use only - no OCPP standard value
    MO_ChargePointStatus_Available,
    MO_ChargePointStatus_Preparing,
    MO_ChargePointStatus_Charging,
    MO_ChargePointStatus_SuspendedEVSE,
    MO_ChargePointStatus_SuspendedEV,
    MO_ChargePointStatus_Finishing,
    MO_ChargePointStatus_Reserved,
    MO_ChargePointStatus_Unavailable,
    MO_ChargePointStatus_Faulted,
#if MO_ENABLE_V201
    MO_ChargePointStatus_Occupied,
#endif
}   MO_ChargePointStatus;

const char *mo_serializeChargePointStatus(MO_ChargePointStatus v);

#ifdef __cplusplus
} //extern "C"

namespace MicroOcpp {

enum class ChangeAvailabilityStatus : uint8_t {
    Accepted,
    Rejected,
    Scheduled
};

} //namespace MicroOcpp
#endif //__cplusplus
#endif //MO_ENABLE_V201
#endif
