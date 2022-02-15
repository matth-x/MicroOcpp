// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <Arduino.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti WiFiMulti;
#elif defined(ESP32)
#include <WiFi.h>
#else
#error only ESP32 or ESP8266 supported at the moment
#endif

#include <ArduinoOcpp.h>

#define STASSID "YOUR_WLAN_SSID"
#define STAPSK  "YOUR_WLAN_PW"

#define OCPP_HOST "echo.websocket.events"
#define OCPP_PORT 80
#define OCPP_URL "ws://echo.websocket.events/"

//
////  Settings which worked for my SteVe instance
//
//#define OCPP_HOST "my.instance.com"
//#define OCPP_PORT 80
//#define OCPP_URL "ws://my.instance.com/steve/websocket/CentralSystemService/gpio-based-charger"

void setup() {

    /*
     * Initialize Serial and WiFi
     */ 

    Serial.begin(115200);
    Serial.setDebugOutput(true);

    Serial.print(F("[main] Wait for WiFi: "));

#if defined(ESP8266)
    WiFiMulti.addAP(STASSID, STAPSK);
    while (WiFiMulti.run() != WL_CONNECTED) {
        Serial.print('.');
        delay(1000);
    }
#elif defined(ESP32)
    WiFi.begin(STASSID, STAPSK);
    while (!WiFi.isConnected()) {
        Serial.print('.');
        delay(1000);
    }
#else
#error only ESP32 or ESP8266 supported at the moment
#endif

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
     * Check internal OCPP state and bind EVSE hardware to it
     */
    if (ocppPermitsCharge()) {
        //OCPP set up and transaction running. Energize the EV plug here
    } else {
        //No transaction running at the moment. De-energize EV plug
    }

    /*
     * Detect if something physical happened at your EVSE and trigger the corresponding OCPP messages
     */
    if (/* RFID chip detected? */ false) {
        const char *idTag = "my-id-tag"; //e.g. idTag = RFID.readIdTag();
        authorize(idTag);
    }
    
    if (/* EV plugged in? */ false) {
        startTransaction("my-id-tag", [] (JsonObject payload) {
            //Callback: Central System has answered. Could flash a confirmation light here.
            Serial.print(F("[main] Started OCPP transaction\n"));
        });
    }
    
    if (/* EV unplugged? */ false) {
        stopTransaction([] (JsonObject payload) {
            //Callback: Central System has answered.
            Serial.print(F("[main] Stopped OCPP transaction\n"));
        });
    }

    //... see ArduinoOcpp.h for more possibilities
}
