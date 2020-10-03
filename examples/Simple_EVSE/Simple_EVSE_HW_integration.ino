// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#include <Variants.h>
#ifndef PROD
#ifdef SIMPLE_EVSE

#include <EVSE.h>
#include <ChargePointStatusService.h>
#include <OcppEngine.h>

#include <Arduino.h>

/*
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

#define EV_PLUG 14
#define EV_PLUGGED HIGH
#define AUTHORIZATION 12
#define AUTHORIZATION_PROVIDED HIGH
#define EV_ENERGY_REQUEST 13
#define EV_REQUESTS_ENERGY HIGH

#define EVSE_RELAY 5
#define EVSE_OFFERS_ENERGY HIGH
#define EVSE_DENIES_ENERGY LOW

OnEvPlug onEvPlug;
OnEvUnplug onEvUnplug;
OnProvideAuthorization onProvideAuthorization;
OnRevokeAuthorization onRevokeAuthorization;
OnBoot onBoot;

bool evIsPlugged = false;
bool authorizationProvided = false;
bool evRequestsEnergy = false;
bool evseOffersEnergy = false;
bool evseIsBooted = false;


float chargingLimit = 3680.f; // = 230V * 16A

void EVSE_initialize() {
  Serial.print(F("[EVSE] Simple_EVSE is being initialized ... "));
  pinMode(EVSE_RELAY, OUTPUT);
  pinMode(EV_PLUG, INPUT);
  pinMode(AUTHORIZATION, INPUT);
  pinMode(EV_ENERGY_REQUEST, INPUT);
  
  onBoot();
  evseIsBooted = true;

  Serial.print(F("ready\n"));
}

void EVSE_loop() {
    if (!evseIsBooted) return;

    if (digitalRead(EV_PLUG) == EV_PLUGGED) {
      if (!evIsPlugged) {
        evIsPlugged = true;
        if (onEvPlug != NULL)
          onEvPlug();
      }
    } else {
      if (evIsPlugged) {
        evIsPlugged = false;
        if (onEvUnplug != NULL)
          onEvUnplug();
      }
    }

    if (digitalRead(AUTHORIZATION) == AUTHORIZATION_PROVIDED) {
      if (!authorizationProvided) {
        authorizationProvided = true;
        if (onProvideAuthorization != NULL)
          onProvideAuthorization();
      }
    } else {
      if (authorizationProvided) {
        authorizationProvided = false;
        if (onRevokeAuthorization != NULL)
          onRevokeAuthorization();
      }
    }

    if (digitalRead(EV_ENERGY_REQUEST) == EV_REQUESTS_ENERGY) {
      if (!evRequestsEnergy) {
        evRequestsEnergy = true;
        if (getChargePointStatusService() != NULL)
            getChargePointStatusService()->startEvDrawsEnergy();
      }
    } else {
      if (evRequestsEnergy) {
        evRequestsEnergy = false;
        if (getChargePointStatusService() != NULL)
            getChargePointStatusService()->stopEvDrawsEnergy();
      }
    }
}

void EVSE_setChargingLimit(float limit) {
  Serial.print(F("[EVSE] New charging limit set. Got "));
  Serial.print(limit);
  Serial.print(F("\n"));
  chargingLimit = limit;
}

void EVSE_startEnergyOffer() {
  if (evseOffersEnergy) {
    Serial.print(F("[EVSE] Warning: called EVSE_startEnergyOffer() though EVSE already offers energy\n"));
  }

  evseOffersEnergy = true;

  digitalWrite(EVSE_RELAY, EVSE_OFFERS_ENERGY);
}

void EVSE_stopEnergyOffer() {
  if (!evseOffersEnergy) {
    Serial.print(F("[EVSE] Warning: called EVSE_startEnergyOffer() though EVSE already offers energy\n"));
  }

  evseOffersEnergy = false;

  digitalWrite(EVSE_RELAY, EVSE_DENIES_ENERGY);
}

float EVSE_readChargeRate() { //estimation for EVSEs without power meter
  if (evIsPlugged && evRequestsEnergy) { //example
    return chargingLimit;
  } else {
    return 0.0f;
  }
}

float EVSE_readEnergyRegister(){
  return 3680.0f * ((float) millis()) * 0.001f; //example values
}

bool EVSE_EvRequestsCharge() {
  return evRequestsEnergy;
}

bool EVSE_EvIsPlugged() {
  return evIsPlugged;
}

bool EVSE_authorizationProvided() {
  return authorizationProvided;
}

void EVSE_setOnEvPlug(OnEvPlug onEvPg) {
  onEvPlug = onEvPg;
}

void EVSE_setOnEvUnplug(OnEvUnplug onEvUpg) {
  onEvUnplug = onEvUpg;
}

void EVSE_setOnProvideAuthorization(OnProvideAuthorization onAuth) {
  onProvideAuthorization = onAuth;
}

void EVSE_setOnRevokeAuthorization(OnRevokeAuthorization onRevokeAuth) {
  onRevokeAuthorization = onRevokeAuth;
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
