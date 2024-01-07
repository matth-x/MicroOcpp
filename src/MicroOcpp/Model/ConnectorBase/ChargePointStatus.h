// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
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

namespace Ocpp201 {

enum class ConnectorStatus {
    Available,
    Occupied,
    Reserved,
    Unavailable,
    Faulted,
    NOT_SET //internal value for "undefined"
};

} //end namespace Ocpp201

} //end namespace MicroOcpp

#endif
