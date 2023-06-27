// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Model/ChargeControl/ChargePointErrorCode.h>

using namespace ArduinoOcpp;

const char *ArduinoOcpp::serializeErrorCode(ErrorCode code) {
    const char *res = "InternalError";
    
    switch (code) {
        case ErrorCode::ConnectorLockFailure:
            res = "ConnectorLockFailure";
            break;
        case ErrorCode::EVCommunicationError:
            res = "EVCommunicationError";
            break;
        case ErrorCode::GroundFailure:
            res = "GroundFailure";
            break;
        case ErrorCode::HighTemperature:
            res = "HighTemperature";
            break;
        case ErrorCode::InternalError:
            res = "InternalError";
            break;
        case ErrorCode::LocalListConflict:
            res = "LocalListConflict";
            break;
        case ErrorCode::NoError:
            res = "NoError";
            break;
        case ErrorCode::OtherError:
            res = "OtherError";
            break;
        case ErrorCode::OverCurrentFailure:
            res = "OverCurrentFailure";
            break;
        case ErrorCode::OverVoltage:
            res = "OverVoltage";
            break;
        case ErrorCode::PowerMeterFailure:
            res = "PowerMeterFailure";
            break;
        case ErrorCode::PowerSwitchFailure:
            res = "PowerSwitchFailure";
            break;
        case ErrorCode::ReaderFailure:
            res = "ReaderFailure";
            break;
        case ErrorCode::ResetFailure:
            res = "ResetFailure";
            break;
        case ErrorCode::UnderVoltage:
            res = "UnderVoltage";
            break;
        case ErrorCode::WeakSignal:
            res = "WeakSignal";
            break;
    }

    return res;
}

ChargePointErrorCode ArduinoOcpp::adaptErrorCode(ChargePointErrorCode_c code) {
    ChargePointErrorCode res = ChargePointErrorCode::InternalError;
    
    switch (code) {
        case ChargePointErrorCode_c::ConnectorLockFailure:
            res = ChargePointErrorCode::ConnectorLockFailure;
            break;
        case ChargePointErrorCode_c::EVCommunicationError:
            res = ChargePointErrorCode::EVCommunicationError;
            break;
        case ChargePointErrorCode_c::GroundFailure:
            res = ChargePointErrorCode::GroundFailure;
            break;
        case ChargePointErrorCode_c::HighTemperature:
            res = ChargePointErrorCode::HighTemperature;
            break;
        case ChargePointErrorCode_c::InternalError:
            res = ChargePointErrorCode::InternalError;
            break;
        case ChargePointErrorCode_c::LocalListConflict:
            res = ChargePointErrorCode::LocalListConflict;
            break;
        case ChargePointErrorCode_c::NoError:
            res = ChargePointErrorCode::NoError;
            break;
        case ChargePointErrorCode_c::OtherError:
            res = ChargePointErrorCode::OtherError;
            break;
        case ChargePointErrorCode_c::OverCurrentFailure:
            res = ChargePointErrorCode::OverCurrentFailure;
            break;
        case ChargePointErrorCode_c::OverVoltage:
            res = ChargePointErrorCode::OverVoltage;
            break;
        case ChargePointErrorCode_c::PowerMeterFailure:
            res = ChargePointErrorCode::PowerMeterFailure;
            break;
        case ChargePointErrorCode_c::PowerSwitchFailure:
            res = ChargePointErrorCode::PowerSwitchFailure;
            break;
        case ChargePointErrorCode_c::ReaderFailure:
            res = ChargePointErrorCode::ReaderFailure;
            break;
        case ChargePointErrorCode_c::ResetFailure:
            res = ChargePointErrorCode::ResetFailure;
            break;
        case ChargePointErrorCode_c::UnderVoltage:
            res = ChargePointErrorCode::UnderVoltage;
            break;
        case ChargePointErrorCode_c::WeakSignal:
            res = ChargePointErrorCode::WeakSignal;
            break;
    }

    return res;
}
