// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
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

} //end namespace ArduinoOcpp
#endif
