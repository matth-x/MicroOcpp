// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#ifndef EVSE_H
#define EVSE_H

#include <Arduino.h>

typedef void (*OnEvPlug)();
typedef void (*OnEvUnplug)();

typedef void (*OnBoot)();

//typedef void (*OnEvRequestsCharge)();
//typedef void (*OnEvSuspended)();



void EVSE_initialize();

void EVSE_setChargingLimit(float limit);
float EVSE_readChargeRate();
float EVSE_readEnergyRegister();

bool EVSE_EvRequestsCharge();

bool EVSE_EvIsPlugged();

void EVSE_setOnEvPlug(OnEvPlug onEvPlug);
void EVSE_setOnEvUnplug(OnEvUnplug onEvUnplug);
void EVSE_setOnBoot(OnBoot onBoot);
void EVSE_loop();

void EVSE_getChargePointSerialNumber(String &out);
char *EVSE_getChargePointVendor();
char *EVSE_getChargePointModel();

#endif
