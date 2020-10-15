// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#ifndef OCPP_EVSE_STATE
#define OCPP_EVSE_STATE

enum class OcppEvseState {
  Available,
  Preparing,
  Charging,
  SuspendedEVSE,
  SuspendedEV,
  Finishing,    //not supported by this client
  Reserved,     //not supported by this client
  Unavailable,  //not supported by this client
  Faulted,      //not supported by this client
  NOT_SET //not part of OCPP 1.6
};

#endif
