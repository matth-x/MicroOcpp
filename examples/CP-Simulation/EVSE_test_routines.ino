// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#include <Variants.h>
#ifndef PROD
#ifndef SIMPLE_EVSE

#include <EVSE.h>
#include <ChargePointStatusService.h>
#include <OcppEngine.h>

#include <Arduino.h>

OnEvPlug onEvPlug;
OnEvUnplug onEvUnplug;
OnBoot onBoot;

bool evIsPlugged = false;       //only example
bool evPluggedBefore;           //only example
bool evRequestsCharge = false;  //only example
bool evRequestedChargeBefore;   //only example
bool evseIsBooted = false;      //only example

ulong T_PLUG = 10000; //after ... ms, user plugs EV
ulong T_FULL = 30000; //after ... ms, EV is fully charged and doesn't request energy anymore
ulong T_UNPLUG = 40000; //after ... ms, user unplugs EV
ulong T_RESET = 60000; //after ... ms, the test routine start over again

ulong t;

float chargingLimit = 0.0f;

void EVSE_initialize() {
  Serial.print(F("[EVSE] CP is being initialized. Start test routines\n"));
  onBoot();
  evseIsBooted = true;

  t = millis();
}

void EVSE_loop() {
    if (!evseIsBooted) return;

    /*
     * Test routines
     */

    if (millis() - t >= T_RESET) {
        t = millis();
        evIsPlugged = false;
        evRequestsCharge = false;
        Serial.print(F("\n[EVSE]      ---   Start test routines   ---\n"));
        return;
    }

    if (millis() - t >= T_UNPLUG) {
        if (evIsPlugged){
            evIsPlugged = false;
            if (onEvUnplug != NULL)
                onEvUnplug();
        }
        return;
    }

    if (millis() - t >= T_FULL) {
        if (evRequestsCharge){
            evRequestsCharge = false;
            if (getChargePointStatusService() != NULL)
                    getChargePointStatusService()->stopEvDrawsEnergy();
        }
        return;
    }

    if (millis() - t >= T_PLUG) {
        if (!evIsPlugged){
            evIsPlugged = true;
            if (onEvPlug != NULL)
                onEvPlug();
                evRequestsCharge = true;
                if (getChargePointStatusService() != NULL)
                    getChargePointStatusService()->startEvDrawsEnergy();
        }
        return;
    }
}

/*
 * @param limit: expects current in amps from 0.0 to 32.0
 */
void EVSE_setChargingLimit(float limit) {
  Serial.print(F("[EVSE] New charging limit set. Got "));
  Serial.print(limit);
  Serial.print(F("\n"));
  chargingLimit = limit;
}

float EVSE_readChargeRate() {
  if (evIsPlugged && evRequestsCharge) { //example
    return chargingLimit;
  } else {
    return 0.0f;
  }
}

float EVSE_readEnergyRegister(){
  return 0.0f; //not used because a custom energy sampler is set in example_client.cpp
}

bool EVSE_EvRequestsCharge() {
  return evRequestsCharge;
}

bool EVSE_EvIsPlugged() {
  return evIsPlugged;
}

void EVSE_setOnEvPlug(OnEvPlug onEvPg) {
  onEvPlug = onEvPg;
}

void EVSE_setOnEvUnplug(OnEvUnplug onEvUpg) {
  onEvUnplug = onEvUpg;
}

void EVSE_setOnBoot(OnBoot onBt) {
  onBoot = onBt;
}

void EVSE_getChargePointSerialNumber(String &out) {
  out += "myCPSerialNumber";
}

char *EVSE_getChargePointVendor() {
  return "Greatest CP vendor";
}

char *EVSE_getChargePointModel() {
  return "Greatest CP model";
}

#endif
#endif
