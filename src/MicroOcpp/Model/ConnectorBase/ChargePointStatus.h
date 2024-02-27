// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef OCPP_EVSE_STATE
#define OCPP_EVSE_STATE

#include <MicroOcpp/Version.h>

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

#if MO_ENABLE_V201
    Occupied,
#endif

    NOT_SET //internal value for "undefined"
};

} //end namespace MicroOcpp

#endif
