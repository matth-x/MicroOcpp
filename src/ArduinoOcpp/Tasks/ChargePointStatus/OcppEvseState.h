// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
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
  Finishing,    //not supported by this client
  Reserved,     //not supported by this client
  Unavailable,
  Faulted,
  NOT_SET //not part of OCPP 1.6
};

} //end namespace ArduinoOcpp
#endif
