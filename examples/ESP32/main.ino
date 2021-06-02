// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <Arduino.h>
#include <WiFi.h>

#include <ArduinoOcpp.h>

#define STASSID "YOUR_WLAN_SSID"
#define STAPSK  "YOUR_WLAN_PW"

#define OCPP_HOST "wss://echo.websocket.org"
#define OCPP_PORT 80
#define OCPP_URL "wss://echo.websocket.org/"

void setup() {

    /*
     * Initialize Serial and WiFi
     */ 

    Serial.begin(115200);
    Serial.setDebugOutput(true);

    WiFi.begin(STASSID, STAPSK);

    Serial.print(F("[main] Wait for WiFi: "));
    while (!WiFi.isConnected()) {
        Serial.print('.');
        delay(1000);
    }
    Serial.print(F(" connected!\n"));

    /*
     * Initialize the OCPP library
     */
    OCPP_initialize(OCPP_HOST, OCPP_PORT, OCPP_URL);

    /*
     * Integrate OCPP functionality. You can leave out the following part if your EVSE doesn't need it.
     */
    setPowerActiveImportSampler([]() {
        //measure the input power of the EVSE here and return the value in Watts
        return 0.f;
    });

    setOnChargingRateLimitChange([](float limit) {
        //set the SAE J1772 Control Pilot value here
        Serial.print(F("[main] Smart Charging allows maximum charge rate: "));
        Serial.println(limit);
    });

    setEvRequestsEnergySampler([]() {
        //return true if the EV is in state "Ready for charging" (see https://en.wikipedia.org/wiki/SAE_J1772#Control_Pilot)
        return false;
    });

    //... see ArduinoOcpp.h for more settings

    /*
     * Notify the Central System that this station is ready
     */
    bootNotification("My Charging Station", "My company name");
}

void loop() {

    /*
     * Do all OCPP stuff (process WebSocket input, send recorded meter values to Central System, etc.)
     */
    OCPP_loop();

    /*
     * Detect if something physical happened at your EVSE and trigger the corresponding OCPP messages
     */
    if (/* RFID chip detected? */ false) {
        String idTag = "my-id-tag"; //e.g. idTag = RFID.readIdTag();
        authorize(idTag);
    }
    
    if (/* EV plugged in? */ false) {
        startTransaction([] (JsonObject payload) {
            //Callback: Central System has answered. Energize your EV plug inside this callback and flash a confirmation light if you want.
            Serial.print(F("[main] Started OCPP transaction. EV plug energized\n"));
        });
    }
    
    if (/* EV plugged out? */ false) {
        stopTransaction([] (JsonObject payload) {
            //Callback: Central System has answered. Energize your EV plug inside this callback and flash a confirmation light if you want.
            Serial.print(F("[main] Started OCPP transaction. EV plug energized\n"));
        });
    }

    //... see ArduinoOcpp.h for more possibilities
}
