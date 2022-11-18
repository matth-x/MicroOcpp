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

#define STASSID "YOUR_WIFI_SSID"
#define STAPSK  "YOUR_WIFI_PW"

#define OCPP_HOST "echo.websocket.events"
#define OCPP_PORT 80
#define OCPP_URL "ws://echo.websocket.events/"

//
//  Settings which worked for my SteVe instance:
//
//#define OCPP_HOST "my.instance.com"
//#define OCPP_PORT 80
//#define OCPP_URL "ws://my.instance.com/steve/websocket/CentralSystemService/esp-charger"

void setup() {

    /*
     * Initialize Serial and WiFi
     */ 

    Serial.begin(115200);

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

    Serial.println(F(" connected!"));

    /*
     * Initialize the OCPP library
     */
    OCPP_initialize(OCPP_HOST, OCPP_PORT, OCPP_URL);

    /*
     * Integrate OCPP functionality. You can leave out the following part if your EVSE doesn't need it.
     */
    setEnergyMeterInput([]() {
        //take the energy register of the main electricity meter and return the value in watt-hours
        return 0.f;
    });

    setSmartChargingOutput([](float limit) {
        //set the SAE J1772 Control Pilot value here
        Serial.printf("[main] Smart Charging allows maximum charge rate: %.0f\n", limit);
    });

    setConnectorPluggedInput([]() {
        //return true if an EV is plugged to this EVSE
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
        String idTag = "0123456789ABCD"; //e.g. idTag = RFID.readIdTag();

        if (!getTransactionIdTag()) {
            //no idTag registered yet. Start a new transaction

            authorize(idTag.c_str(), [idTag] (JsonObject response) {
                //check if user with idTag is authorized
                if (!strcmp("Accepted", response["idTagInfo"]["status"] | "Invalid")){
                    Serial.println(F("[main] User is authorized to start a transaction"));

                    auto ret = beginTransaction(idTag.c_str()); //begin Tx locally

                    if (ret) {
                        Serial.println(F("[main] Transaction initiated. StartTransaction will be sent when ConnectorPlugged Input becomes true"));
                    } else {
                        Serial.println(F("[main] No transaction initiated"));
                    }
                } else {
                    Serial.printf("[main] Authorize denied. Reason: %s\n", response["idTagInfo"]["status"] | "");
                }
            });
            Serial.printf("[main] Authorizing user with idTag %s\n", idTag.c_str());
        } else {
            //Transaction already initiated. Check if to stop current Tx by RFID card
            if (idTag.equals(getTransactionIdTag())) {
                //card matches -> user can stop Tx
                Serial.println(F("[main] End transaction by RFID card"));

                endTransaction();
            } else {
                Serial.println(F("[main] Cannot end transaction by RFID card (different card?)"));
            }
        }
    }

    //... see ArduinoOcpp.h for more possibilities
}
