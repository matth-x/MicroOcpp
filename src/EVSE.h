#ifndef EVSE_H
#define EVSE_H

typedef void (*OnEvPlug)();
typedef void (*OnEvUnplug)();

typedef void (*OnBoot)();

//typedef void (*OnEvRequestsCharge)();
//typedef void (*OnEvSuspended)();



void EVSE_initialize();

void EVSE_setChargingLimit(float limit);
float EVSE_readChargeRate();

bool EVSE_EvRequestsCharge();

bool EVSE_EvIsPlugged();

void EVSE_setOnEvPlug(OnEvPlug onEvPlug);
void EVSE_setOnEvUnplug(OnEvUnplug onEvUnplug);
void EVSE_setOnBoot(OnBoot onBoot);
void EVSE_loop();

char *EVSE_getChargePointSerialNumber();
char *EVSE_getChargePointVendor();
char *EVSE_getChargePointModel();

#endif
