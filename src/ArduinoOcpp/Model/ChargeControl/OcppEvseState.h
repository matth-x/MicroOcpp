// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef OCPP_EVSE_STATE
#define OCPP_EVSE_STATE

namespace ArduinoOcpp {

enum class OcppEvseState {
    Available,
    Preparing,
    Charging,
    SuspendedEVSE,
    SuspendedEV,
    Finishing,
    Reserved,
    Unavailable,
    Faulted,
    NOT_SET //internal value for "undefined"
};

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

} //end namespace ArduinoOcpp
#endif
