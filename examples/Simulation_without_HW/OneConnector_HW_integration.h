// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef ONE_CONNECTOR_HW_INTEGRATION
#define ONE_CONNECTOR_HW_INTEGRATION

void EVSE_initialize();

void EVSE_loop();

float EVSE_readChargeRate();

void EVSE_setChargingLimit(float limit);

bool EVSE_evRequestsEnergy();

#endif
