// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <Variants.h>

#include "OneConnector_HW_integration.h"

#include <Arduino.h>

#include <SingleConnectorEvseFacade.h>

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

#define EV_PLUG 14
#define EV_PLUGGED HIGH
#define AUTHORIZATION 12
#define AUTHORIZATION_PROVIDED HIGH
#define EV_ENERGY_REQUEST 13
#define EV_REQUESTS_ENERGY HIGH

#define EVSE_RELAY 5
#define EVSE_OFFERS_ENERGY HIGH
#define EVSE_DENIES_ENERGY LOW

void onEvPlug();
void onEvUnplug();
void onProvideAuthorization();
void onRevokeAuthorization();

bool evIsPlugged = false;
bool transactionRunning = false;
bool authorizationProvided = false;
bool successfullyAuthorized = false;
bool evRequestsEnergy = false;
bool evseIsBooted = false;


float chargingLimit = 3680.f; // = 230V * 16A

void EVSE_initialize() {
  Serial.print(F("[EVSE] Simple_EVSE is being initialized ... "));
  pinMode(EVSE_RELAY, OUTPUT);
  pinMode(EV_PLUG, INPUT);
  pinMode(AUTHORIZATION, INPUT);
  pinMode(EV_ENERGY_REQUEST, INPUT);
  
  bootNotification("GPIO-based CP model", "Greatest EVSE vendor", [] (JsonObject confMsg) {
    //This callback is executed when the .conf() response from the central system arrives
    Serial.print(F("BootNotification was answered. Central system clock: "));
    Serial.print(confMsg["currentTime"].as<String>());
    Serial.print(F("\n"));

    evseIsBooted = true;
  });

  Serial.print(F("ready. Wait for BootNotification.conf(), then start\n"));
}

void EVSE_loop() {
    if (!evseIsBooted) return;

    if (digitalRead(EV_PLUG) == EV_PLUGGED) {
      if (!evIsPlugged) {
        evIsPlugged = true;
        onEvPlug();
      }
    } else {
      if (evIsPlugged) {
        evIsPlugged = false;
        onEvUnplug();
      }
    }

    if (digitalRead(AUTHORIZATION) == AUTHORIZATION_PROVIDED) {
      if (!authorizationProvided) {
        authorizationProvided = true;
        onProvideAuthorization();
      }
    } else {
      if (authorizationProvided) {
        authorizationProvided = false;
        onRevokeAuthorization();
      }
    }

    if (digitalRead(EV_ENERGY_REQUEST) == EV_REQUESTS_ENERGY) {
      if (!evRequestsEnergy) {
        evRequestsEnergy = true;
        startEvDrawsEnergy(); //Notify ChargePointStatusService about the new EVSE state. It then sends a Status Notification
      }
    } else {
      if (evRequestsEnergy) {
        evRequestsEnergy = false;
        stopEvDrawsEnergy(); //Notify ChargePointStatusService about the new EVSE state. It then sends a Status Notification
      }
    }
}

void onEvPlug() {
    Serial.print(F("[EVSE] EV plugged\n"));
    if (successfullyAuthorized) {
        startTransaction([](JsonObject conf) {
            transactionRunning = true;
            digitalWrite(EVSE_RELAY, EVSE_OFFERS_ENERGY);
        });
    }
}

void onEvUnplug() {
    Serial.print(F("[EVSE] EV unplugged\n"));
    if (transactionRunning) {
        stopTransaction();
    }
    digitalWrite(EVSE_RELAY, EVSE_DENIES_ENERGY);
    transactionRunning = false;
    successfullyAuthorized = false;
}

void onProvideAuthorization() {
    Serial.print(F("[EVSE] User authorized charging session\n"));
    String idTag = String("abcdef123456789"); // e.g. idTag = readRFIDTag();

    /*
     * Demonstrate the use of all event listeners that come with sending OCPP operations. At the beginning, you probably only need
     * the first listener which the engine fires when the Central System has responded with a .conf() msg.
     */
    authorize(idTag, [](JsonObject conf) {
        //execute when the .conf() response from the central system arrives (optional but very likely necessary for your integration)
        successfullyAuthorized = true;

        if (evIsPlugged) {
            startTransaction([](JsonObject conf) {
                transactionRunning = true;
                digitalWrite(EVSE_RELAY, EVSE_OFFERS_ENERGY);
            });
        }
    }, []() {
        //add further listeners (optional)
        Serial.print(F("[EVSE] Could not authorize charging session! Aborted\n"));
    }, []() { //(optional)
        Serial.print(F("[EVSE] Could not authorize charging session! Reason: timeout\n"));
    }, [](const char *code, const char *description, JsonObject details) { //(optional)
        Serial.print(F("[EVSE] Could not authorize charging session! Reason: received OCPP error: "));
        Serial.println(code);
    });
}

void onRevokeAuthorization() {
    Serial.print(F("[EVSE] User revoked charging session authorization\n"));
    successfullyAuthorized = false;
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

    if (chargingLimit <= 0.f) {
        digitalWrite(EVSE_RELAY, EVSE_DENIES_ENERGY);
    } else {
        if (transactionRunning) {
            digitalWrite(EVSE_RELAY, EVSE_OFFERS_ENERGY);
        }
    }
}
