#include "EVSE.h"

#include <Arduino.h>

#define byte uint8_t

OnEvPlug onEvPlug;
OnEvUnplug onEvUnplug;
OnBoot onBoot;

bool evIsPlugged = false;       //only example
bool evPluggedBefore;           //only example
bool evRequestsCharge = false;  //only example
bool evRequestedChargeBefore;   //only example

byte chargingLimit = 0x00; //from 0 (=0%) to 100 (=100%); //only example
byte chargingLimitBefore = 0x00;//only example

bool evseIsBooted = false;      //only example

void EVSE_initialize() {
  Serial.print(F("[EVSE] CP is being initialized\n"));
}

void EVSE_loop() {

/* This code gives an example of how the Eventlisteners could be used
    if ( ... ) { //Boot signal detected
      if (!evseIsBooted) {
        evseIsBooted = true;
        if (onBoot != NULL) {
          onBoot();
          onBoot = NULL; //only call once
        }
      }
    } else if ( ... ) { //Plug detected
      
      if (!evIsPlugged && evPluggedRead) {
        evIsPlugged = true;
        if (onEvPlug != NULL) {
          onEvPlug();
        }
      }
    } else if ( ... ) { //Unplug detected
  
      if (evIsPlugged && !evPluggedRead) {
        evIsPlugged = false;
        if (onEvUnplug != NULL) {
          onEvUnplug();
        }
      }
    }

*/
}

/*
 * @param limit: expects current in amps from 0.0 to 32.0
 */
void EVSE_setChargingLimit(float limit) {
  Serial.print(F("[EVSE] New charging limit set. Got "));
  Serial.print(limit);
  Serial.print(F(" = "));
  float percentage = 100.0f * limit / 32.0f; //from 0 to 100
  byte ceiledPercentage = (byte) (percentage + 0.9f);
  chargingLimit = ceiledPercentage;
  Serial.print(chargingLimit, DEC);
  Serial.print(F("% of 32 amps"));
}

float EVSE_readChargeRate() {
  //if (evIsPlugged && evRequestsCharge) { //example
  if (1) {
    return ((float) chargingLimit) * 32.0f / 100.f;
  } else {
    return 0.0f;
  }
}

bool EVSE_EvRequestsCharge() {
  //return evRequestsCharge;
  return 1;
}

bool EVSE_EvIsPlugged() {
  //return evIsPlugged;
  return 1;
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

char *EVSE_getChargePointSerialNumber() {
  return "myCPSerialNumber";
}

char *EVSE_getChargePointVendor() {
  return "Greatest CP vendor";
}

char *EVSE_getChargePointModel() {
  return "Greatest CP model";
}
