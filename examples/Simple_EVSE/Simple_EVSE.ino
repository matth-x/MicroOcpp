// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#include <Variants.h>
#ifndef PROD
#ifdef SIMPLE_EVSE

#include <Arduino.h>

#include <WebSocketsClient.h>

#include <ArduinoJson.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

//ESP8266-OCPP modules
#include <OcppEngine.h>
#include <SmartChargingService.h>
#include <ChargePointStatusService.h>
#include <MeteringService.h>
#include <TimeHelper.h>
#include <EVSE.h>
#include <SimpleOcppOperationFactory.h>

//OCPP message classes
#include <OcppOperation.h>
#include <OcppMessage.h>
#include <Authorize.h>
#include <BootNotification.h>
#include <Heartbeat.h>
#include <StartTransaction.h>
#include <StopTransaction.h>
#include <DataTransfer.h>

ESP8266WiFiMulti WiFiMulti;

WebSocketsClient webSocket;

SmartChargingService *smartChargingService;
ChargePointStatusService *chargePointStatusService;
MeteringService *meteringService;

#define STASSID "WLAN040"                                 // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#define STAPSK  "sh10j1978"                              // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!

#define WS_HOST "wss://echo.websocket.org"
#define WS_PORT 80
#define WS_URL_PREFIX "wss://echo.websocket.org/"
#define WS_PROTOCOL "ocpp1.6"

#define LED 2
//#define LED 13
#define LED_ON LOW
#define LED_OFF HIGH


/*
   Called by Websocket library on incoming message on the internet link
*/
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

  switch (type) {
    case WStype_DISCONNECTED:
      Serial.print(F("[WSc] Disconnected!\n"));
      break;
    case WStype_CONNECTED:
      Serial.printf("[WSc] Connected to url: %s\n", payload);
      break;
    case WStype_TEXT:
      if (DEBUG_OUT) Serial.printf("[WSc] get text: %s\n", payload);

      if (!processWebSocketEvent(payload, length)) { //forward message to OcppEngine
        Serial.print(F("[WSc] Processing WebSocket input event failed!\n"));
      }
      break;
    case WStype_BIN:
      Serial.print(F("[WSc] Incoming binary data stream not supported"));
      break;
    case WStype_PING:
      // pong will be send automatically
      Serial.print(F("[WSc] get ping\n"));
      break;
    case WStype_PONG:
      // answer to a ping we send
      Serial.print(F("[WSc] get pong\n"));
      break;
  }
}

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

  // server address, port and URL
  String url = String(WS_URL_PREFIX);
  String cpSerial = String('\0');
  EVSE_getChargePointSerialNumber(cpSerial);
  //url += cpSerial; //most OCPP-Server require URLs like this. Since we're testing with an echo server here, this is obsolete
  webSocket.begin(WS_HOST, WS_PORT, url, WS_PROTOCOL);

  // event handler
  webSocket.onEvent(webSocketEvent);

  // use HTTP Basic Authorization this is optional remove if not needed
  // webSocket.setAuthorization("user", "Password");

  // try ever 5000 again if connection has failed
  webSocket.setReconnectInterval(5000);

  // start heartbeat (optional)
  // ping server every 15000 ms
  // expect pong from server within 3000 ms
  // consider connection disconnected if pong is not received 2 times
  webSocket.enableHeartbeat(15000, 3000, 2); //comment this one out to for specific OCPP servers

  ocppEngine_initialize(&webSocket, 2048); //default JSON document size = 2048

  //Example for SmartCharging Service usage
  smartChargingService = new SmartChargingService(16.0f); //default charging limit: 16A
  smartChargingService->setOnLimitChange([](float limit) {
    if (DEBUG_OUT) Serial.print(F("[SmartChargingService] Limit Change arrived at Callback function: new limit = "));
    if (DEBUG_OUT) Serial.print(limit);
    if (DEBUG_OUT) Serial.print(F(" A\n"));
    EVSE_setChargingLimit(limit);
  });

  chargePointStatusService = new ChargePointStatusService(&webSocket); //adds itself to ocppEngine in constructor
  meteringService = new MeteringService(&webSocket);

  //set system time to default value; will be without effect as soon as the BootNotification conf arrives
  setTimeFromJsonDateString("2019-11-01T11:59:55.123Z"); //use if needed for debugging

  meteringService->setPowerSampler([]() {
    return EVSE_readChargeRate(); //example values. Put your own power meter in here
  });
  //meteringService->setPowerSampler(EVSE_readChargeRate); //Alternative

  meteringService->setEnergySampler([]() {
    return EVSE_readEnergyRegister();
  });


  EVSE_setOnBoot([]() {
    OcppOperation *bootNotification = makeOcppOperation(&webSocket,
        new BootNotification());
    initiateOcppOperation(bootNotification);
    bootNotification->setOnReceiveConfListener([](JsonObject payload) {
        if (DEBUG_OUT) Serial.print(F("BootNotification at client callback.\n"));
    });
  });

  EVSE_setOnEvPlug([] () {
    if (DEBUG_OUT) Serial.print(F("EVSE_setOnEvPlug Callback: EV plugged!\n"));

    if (!EVSE_authorizationProvided()) {
      if (DEBUG_OUT) Serial.print(F("EVSE_setOnEvPlug Callback: no authorization provided yet. Wait for authorization\n"));
      return;
    }

    OcppOperation *startTransaction = makeOcppOperation(&webSocket,
      new StartTransaction());
    initiateOcppOperation(startTransaction);
    startTransaction->setOnReceiveConfListener([](JsonObject payload) {
      Serial.print(F("EVSE_setOnEvPlug Callback: StartTransaction was successful\n"));
      digitalWrite(LED, LED_ON);
      EVSE_startEnergyOffer();
    });
  });

  EVSE_setOnEvUnplug([] () {
    if (chargePointStatusService->getTransactionId() < 0) {
      //there is currently no transaction running. Do nothing
      return;
    }

    OcppOperation *stopTransaction = makeOcppOperation(&webSocket,
          new StopTransaction());
    initiateOcppOperation(stopTransaction);
    stopTransaction->setOnReceiveConfListener([](JsonObject payload) {
      Serial.print(F("EVSE_setOnEvUnplug Callback: StopTransaction was successful\n"));
      digitalWrite(LED, LED_OFF);
      EVSE_stopEnergyOffer();
    });
  });

  EVSE_setOnProvideAuthorization([] () {
    if (DEBUG_OUT) Serial.print(F("EVSE_setOnProvideAuthorization Callback: EV plugged!\n"));

    String myIdTag = "fedcba987"; //use fixed ID tag in this implementation
    OcppOperation *authorize = makeOcppOperation(&webSocket,
        new Authorize(myIdTag));
    initiateOcppOperation(authorize);

    authorize->setOnReceiveConfListener([](JsonObject payload) {
      if (DEBUG_OUT) Serial.print(F("EVSE_setOnProvideAuthorization Callback: Authorize request has been accepted!\n"));

      if (!EVSE_EvIsPlugged()) {
        //There is no EV present so do nothing further
        if (DEBUG_OUT) Serial.print(F("EVSE_setOnProvideAuthorization Callback: No EV plugged. No further action\n"));
        return;
      }

      OcppOperation *startTransaction = makeOcppOperation(&webSocket,
        new StartTransaction());
      initiateOcppOperation(startTransaction);
      startTransaction->setOnReceiveConfListener([](JsonObject payload) {
        Serial.print(F("EVSE_setOnProvideAuthorization Callback: StartTransaction was successful\n"));
        digitalWrite(LED, LED_ON);
        EVSE_startEnergyOffer();
      });
    });
  });

  EVSE_setOnRevokeAuthorization([] {
    if (chargePointStatusService->getTransactionId() < 0) {
      //there is currently no transaction running. Do nothing
      return;
    }

    OcppOperation *stopTransaction = makeOcppOperation(&webSocket,
          new StopTransaction());
    initiateOcppOperation(stopTransaction);
    stopTransaction->setOnReceiveConfListener([](JsonObject payload) {
      Serial.print(F("EVSE_setOnEvUnplug Callback: StopTransaction was successful\n"));
      digitalWrite(LED, LED_OFF);
      EVSE_stopEnergyOffer();
    });
  });

  EVSE_initialize();
  
} //end of setup()

void loop() {
  EVSE_loop();                        //necessary if the implementation of EVSE.h demands it
  webSocket.loop();                   //mandatory
  ocppEngine_loop();                  //mandatory
  //smartChargingService->loop();     //optional
  chargePointStatusService->loop();   //optional
  //meteringService->loop();          //optional
}

#endif
#endif
