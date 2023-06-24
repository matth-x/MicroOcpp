// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef AO_CHARGEPOINTERRORCODE_H
#define AO_CHARGEPOINTERRORCODE_H

namespace ArduinoOcpp {

struct ErrorCode {
    bool isError = false; //if any error information is set
    bool isFaulted = false; //if this is a severe error and the EVSE should go into the faulted state
    const char *errorCode = nullptr; //error code reported by the ChargePoint
    const char *info = nullptr; //Additional free format information related to the error
    const char *vendorId = nullptr; //vendor-specific implementation identifier
    const char *vendorErrorCode = nullptr; //vendor-specific error code

    ErrorCode() = default;

    ErrorCode(const char *errorCode) : errorCode(errorCode) {
        if (errorCode) {
            isError = true;
            isFaulted = true;
        }
    }
};

}

#endif
