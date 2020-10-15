// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#include <Variants.h>

#include "OneConnector_HW_integration.h"

#include <Arduino.h>

#include <SingleConnectorEvseFacade.h>

/*
 * This module simulates user interaction at an EVSE. It runs a routine in which a
 * simulated user connects an EV to the EVSE connector, authorizes herself and
 * unplugs the EV again. This routine is repeated over and over again.
 * 
 * The goal of this module is to allow the exploration of this library without the
 * need to write test code before. 
 */

void onEvPlug();
void onEvUnplug();

bool evIsPlugged = false;
bool transactionRunning = false;
bool evRequestsEnergy = false;
bool evseIsBooted = false;

const ulong T_PLUG = 10000; //after ... ms, user plugs EV. This simulation is configured to have authorize the
                      //charging session with a fixed idTag. This is often a good choice for private chargers
const ulong T_FULL = 30000; //after ... ms, EV is fully charged and doesn't request energy anymore
const ulong T_UNPLUG = 40000; //after ... ms, user unplugs EV
const ulong T_RESET = 60000; //after ... ms, the test routine start over again

ulong t;

float chargingLimit = 3680.f; // = 230V * 16A

void EVSE_initialize() {
  Serial.print(F("[EVSE] Simulated_EVSE is being initialized ... "));
  
  bootNotification("Greatest EVSE vendor", "Simulated CP model", [] (JsonObject confMsg) {
    //This callback is executed when the .conf() response from the central system arrives
    Serial.print(F("BootNotification was answered at "));
    Serial.print(confMsg["currentTime"].as<String>());
    Serial.print(F("\n"));

    evseIsBooted = true;
    t = millis();
  });

  Serial.print(F("ready. Wait for BootNotification.conf(), then start\n"));
}

void EVSE_loop() {
    if (!evseIsBooted) return;

    if (!evIsPlugged && millis() - t >= T_PLUG && millis() - t < T_FULL) {
        evIsPlugged = true;
        onEvPlug();
    }
    
    if (evRequestsEnergy && millis() - t >= T_FULL && millis() - t < T_UNPLUG) {
        evRequestsEnergy = false;
        stopEvDrawsEnergy();
    }

    if (evIsPlugged && millis() - t >= T_UNPLUG && millis() - t < T_RESET) {
        evIsPlugged = false;
        onEvUnplug();
    }

    if (millis() - t >= T_RESET) {
        t = millis();
        evIsPlugged = false;
        evRequestsEnergy = false;
        transactionRunning = false;
        Serial.print(F("\n[EVSE]      ---   Start test routines   ---\n"));
        return;
    }
}

void onEvPlug() {
    Serial.print(F("[EVSE] EV plugged\n"));
    String fixedIdTag = String("fefe1d1d");
    authorize(fixedIdTag, [](JsonObject confMsg) {
        startTransaction([](JsonObject conf) {
            transactionRunning = true;
            evRequestsEnergy = true;
            startEvDrawsEnergy();
            Serial.print(F("[EVSE] Successfully authorized and started transaction\n"));
        });
    });
}

void onEvUnplug() {
    Serial.print(F("[EVSE] EV unplugged\n"));
    transactionRunning = false;
    evRequestsEnergy = false;
    stopTransaction();
}

float EVSE_readChargeRate() { //estimation for EVSEs without power meter
    if (evIsPlugged && evRequestsEnergy) { //example
        return chargingLimit;
    } else {
        return 0.0f;
    }
}

void EVSE_setChargingLimit(float limit) {
    Serial.print(F("[EVSE] New charging limit set. Got "));
    Serial.print(limit);
    Serial.print(F("\n"));
    chargingLimit = limit;
}
