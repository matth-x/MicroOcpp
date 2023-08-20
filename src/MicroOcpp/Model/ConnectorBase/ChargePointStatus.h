// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef OCPP_EVSE_STATE
#define OCPP_EVSE_STATE

namespace MicroOcpp {

enum class ChargePointStatus {
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

} //end namespace MicroOcpp

#endif
