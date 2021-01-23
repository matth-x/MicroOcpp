// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <Variants.h>

#include <Arduino.h>

#include <ArduinoJson.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

//ESP8266-OCPP modules
#include <ESP8266-OCPP.h>

//GPIO-based HW integration (used as example)
#include "OneConnector_HW_integration.h"

ESP8266WiFiMulti WiFiMulti;

#define STASSID "YOUR_WLAN_SSID"
#define STAPSK  "YOUR_WLAN_PW"

//  WebSocket echo server test

#define OCPP_HOST "wss://echo.websocket.org"
#define OCPP_PORT 80
#define OCPP_URL "wss://echo.websocket.org/"

//
////  Settings which worked for my SteVe instance
//
//#define OCPP_HOST "my.instance.com"
//#define OCPP_PORT 80
//#define OCPP_URL "ws://my.instance.com/steve/websocket/CentralSystemService/gpio-based-charger"

#define LED 2
#define LED_ON LOW
#define LED_OFF HIGH

void setup() {

  Serial.begin(115200);
  Serial.setDebugOutput(true);

  pinMode(LED, OUTPUT);

  Serial.println();
  Serial.println();
  Serial.println();

  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] BOOT WAIT %d...\n", t);
    Serial.flush();
    digitalWrite(LED, LED_ON);
    delay(100);
    digitalWrite(LED, LED_OFF);
    delay(300);
    digitalWrite(LED, LED_ON);
    delay(300);
    digitalWrite(LED, LED_OFF);
    delay(300);
  }
  
  WiFiMulti.addAP(STASSID, STAPSK);

  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
  }

  OCPP_initialize(OCPP_HOST, OCPP_PORT, OCPP_URL);

  setPowerActiveImportSampler([]() {
    return EVSE_readChargeRate(); //from the GPIO-charger example
  });

  setOnChargingRateLimitChange([](float limit) {
    EVSE_setChargingLimit(limit); //from the GPIO-charger example
  });

  setEvRequestsEnergySampler([]() {
    return EVSE_evRequestsEnergy();
  });

  EVSE_initialize();
}

void loop() {
  OCPP_loop();

  EVSE_loop(); //refers to the GPIO-charger example
}
