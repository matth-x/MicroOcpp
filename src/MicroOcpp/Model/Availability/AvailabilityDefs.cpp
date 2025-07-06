// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Availability/AvailabilityDefs.h>

#include <string.h>

#if MO_ENABLE_V16

void mo_errorData_init(MO_ErrorData *errorData) {
    memset(errorData, 0, sizeof(*errorData));
    errorData->severity = 1;
}

void mo_errorData_setErrorCode(MO_ErrorData *errorData, const char *errorCode) {
    errorData->errorCode = errorCode;
    if (errorCode) {
        errorData->isError = true;
        errorData->isFaulted = true;
    }
}

void mo_errorDataInput_init(MO_ErrorDataInput *errorDataInput) {
    memset(errorDataInput, 0, sizeof(*errorDataInput));
}

#endif //MO_ENABLE_V16

#if MO_ENABLE_V16 || MO_ENABLE_V201

const char *mo_serializeChargePointStatus(MO_ChargePointStatus v) {
    const char *res = "";
    switch (v) {
        case (MO_ChargePointStatus_UNDEFINED):
            res = "";
            break;
        case (MO_ChargePointStatus_Available):
            res = "Available";
            break;
        case (MO_ChargePointStatus_Preparing):
            res = "Preparing";
            break;
        case (MO_ChargePointStatus_Charging):
            res = "Charging";
            break;
        case (MO_ChargePointStatus_SuspendedEVSE):
            res = "SuspendedEVSE";
            break;
        case (MO_ChargePointStatus_SuspendedEV):
            res = "SuspendedEV";
            break;
        case (MO_ChargePointStatus_Finishing):
            res = "Finishing";
            break;
        case (MO_ChargePointStatus_Reserved):
            res = "Reserved";
            break;
        case (MO_ChargePointStatus_Unavailable):
            res = "Unavailable";
            break;
        case (MO_ChargePointStatus_Faulted):
            res = "Faulted";
            break;
#if MO_ENABLE_V201
        case (MO_ChargePointStatus_Occupied):
            res = "Occupied";
            break;
#endif
    }
    return res;
}

#endif //MO_ENABLE_V16 || MO_ENABLE_V201
