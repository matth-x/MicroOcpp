// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

/*
 * Demonstration video: https://youtu.be/hfTh9GjG-N4
 * 
 * The simple EVSE uses GPIO for the communication between the ESP8266 and the peripherals. The sketch was
 * written and tested on the NodeMCU v3. The following pin-mapping shows how the NodeMCU was connected
 * to its peripherals. Feel free to adapt it according to your own setup.
 * 
 * Peripheral        | IN/OUT | Pin name | GPIO No | Meaning of HIGH / LOW
 * -----------------------------------------------------------------------
 * EV_PLUG             INPUT    D5         14        HIGH: EV is plugged
 * AUTHORIZATION       INPUT    D6         12        HIGH: Charging session is authorized
 * EV_ENERGY_REQUEST   INPUT    D7         13        HIGH: The EV is ready to charge and requests the EVSE to offer energy
 * EVSE_ENERGY_OFFER   OUTPUT   D1         5         HIGH: The relay is closed, the charging wire supplies energy
 */

#ifndef ONE_CONNECTOR_HW_INTEGRATION
#define ONE_CONNECTOR_HW_INTEGRATION

void EVSE_initialize();

void EVSE_loop();

float EVSE_readChargeRate();

void EVSE_setChargingLimit(float limit);

bool EVSE_evRequestsEnergy();

#endif
