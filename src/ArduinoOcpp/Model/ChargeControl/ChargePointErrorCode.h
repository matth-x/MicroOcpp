// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef AO_CHARGEPOINTERRORCODE_H
#define AO_CHARGEPOINTERRORCODE_H

#ifdef __cplusplus
extern "C" {
#endif

enum ChargePointErrorCode_c {
    ConnectorLockFailure,
    EVCommunicationError,
    GroundFailure,
    HighTemperature,
    InternalError,
    LocalListConflict,
    NoError,
    OtherError,
    OverCurrentFailure,
    OverVoltage,
    PowerMeterFailure,
    PowerSwitchFailure,
    ReaderFailure,
    ResetFailure,
    UnderVoltage,
    WeakSignal
};

#ifdef __cplusplus
}

namespace ArduinoOcpp {

enum class ChargePointErrorCode {
    ConnectorLockFailure,
    EVCommunicationError,
    GroundFailure,
    HighTemperature,
    InternalError,
    LocalListConflict,
    NoError,
    OtherError,
    OverCurrentFailure,
    OverVoltage,
    PowerMeterFailure,
    PowerSwitchFailure,
    ReaderFailure,
    ResetFailure,
    UnderVoltage,
    WeakSignal
};

using ErrorCode = ChargePointErrorCode;

const char *serializeErrorCode(ErrorCode code);
ChargePointErrorCode adaptErrorCode(ChargePointErrorCode_c code);

struct ErrorData {
    bool isError = false; //if any error information is set
    bool isFaulted = false; //if this is a severe error and the EVSE should go into the faulted state
    ErrorCode errorCode = ErrorCode::NoError; //error code reported by the ChargePoint
    const char *info = nullptr; //Additional free format information related to the error
    const char *vendorId = nullptr; //vendor-specific implementation identifier
    const char *vendorErrorCode = nullptr; //vendor-specific error code

    ErrorData() = default;

    ErrorData(ErrorCode errorCode) : errorCode(errorCode) {
        if (errorCode != ErrorCode::NoError) {
            isError = true;
            isFaulted = true;
        }
    }
};

}
#endif //__cplusplus

#endif
