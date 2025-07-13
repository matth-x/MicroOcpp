// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
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

#include <MicroOcpp.h>

#define STASSID "YOUR_WIFI_SSID"
#define STAPSK  "YOUR_WIFI_PW"

#define OCPP_BACKEND_URL   "ws://echo.websocket.events"
#define OCPP_CHARGE_BOX_ID ""

//
//  Settings which worked for my SteVe instance:
//
//#define OCPP_BACKEND_URL   "ws://192.168.178.100:8080/steve/websocket/CentralSystemService"
//#define OCPP_CHARGE_BOX_ID "esp-charger"

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
    mo_initialize();

    mo_setWebsocketUrl(
            OCPP_BACKEND_URL,   //OCPP backend URL
            OCPP_CHARGE_BOX_ID, //ChargeBoxId, assigned by OCPP backend
            nullptr,            //Authorization key (`nullptr` disables Basic Auth)
            nullptr);           //OCPP backend TLS certificate (`nullptr` disables certificate check)

    mo_setBootNotificationData("My Charging Station", "My company name");

    mo_setOcppVersion(MO_OCPP_V16); //Run OCPP 1.6. For v2.0.1, pass argument `MO_OCPP_V201`

    /*
     * Integrate OCPP functionality. You can leave out the following part if your EVSE doesn't need it.
     */
    mo_setEnergyMeterInput([]() {
        //take the energy register of the main electricity meter and return the value in watt-hours
        return 0;
    });

    mo_setSmartChargingCurrentOutput([](float limit) {
        //set the SAE J1772 Control Pilot value here
        Serial.printf("[main] Smart Charging allows maximum charge rate: %.0f\n", limit);
    });

    mo_setConnectorPluggedInput([]() {
        //return true if an EV is plugged to this EVSE
        return false;
    });

    //... see MicroOcpp.h for more settings
}

void loop() {

    /*
     * Do all OCPP stuff (process WebSocket input, send recorded meter values to Central System, etc.)
     */
    mo_loop();

    /*
     * Energize EV plug if OCPP transaction is up and running
     */
    if (mo_ocppPermitsCharge()) {
        //OCPP set up and transaction running. Energize the EV plug here
    } else {
        //No transaction running at the moment. De-energize EV plug
    }

    /*
     * Use NFC reader to start and stop transactions
     */
    if (/* RFID chip detected? */ false) {
        String idTag = "0123456789ABCD"; //e.g. idTag = RFID.readIdTag();

        if (!mo_getTransactionIdTag()) {
            //no transaction running or preparing. Begin a new transaction
            Serial.printf("[main] Begin Transaction with idTag %s\n", idTag.c_str());

            /*
             * Begin Transaction. The OCPP lib will prepare transaction by checking the Authorization
             * and listen to the ConnectorPlugged Input. When the Authorization succeeds and an EV
             * is plugged, the OCPP lib will send the StartTransaction
             */
            auto ret = mo_beginTransaction(idTag.c_str());

            if (ret) {
                Serial.println(F("[main] Transaction initiated. OCPP lib will send a StartTransaction when" \
                                 "ConnectorPlugged Input becomes true and if the Authorization succeeds"));
            } else {
                Serial.println(F("[main] No transaction initiated"));
            }

        } else {
            //Transaction already initiated. Attempt to stop current Tx by RFID card. If idTag is the same,
            //or in the same group, then MO will stop the transaction and `mo_ocppPermitsCharge()` becomes
            //false
            mo_endTransaction(idTag.c_str(), nullptr); //`idTag` and `reason` can be nullptr
        }
    }

    //... see MicroOcpp.h for more possibilities
}
