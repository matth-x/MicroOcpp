// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef MO_CHARGEPOINTERRORCODE_H
#define MO_CHARGEPOINTERRORCODE_H

namespace MicroOcpp {

struct ErrorData {
    bool isError = false; //if any error information is set
    bool isFaulted = false; //if this is a severe error and the EVSE should go into the faulted state
    const char *errorCode = nullptr; //see ChargePointErrorCode (p. 76/77) for possible values
    const char *info = nullptr; //Additional free format information related to the error
    const char *vendorId = nullptr; //vendor-specific implementation identifier
    const char *vendorErrorCode = nullptr; //vendor-specific error code

    ErrorData() = default;

    ErrorData(const char *errorCode = nullptr) : errorCode(errorCode) {
        if (errorCode) {
            isError = true;
            isFaulted = true;
        }
    }
};

}

#endif
