#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <WebSocketsClient.h>

#include <Hash.h>

#include<ArduinoJson.h>

#include <string.h>

//ESP8266-OCPP modules
#include <OcppEngine.h>
#include <SmartChargingService.h>
#include <ChargePointStatusService.h>
#include <MeteringService.h>
#include <TimeHelper.h>
#include <EVSE.h>

//OCPP message classes
#include <OcppOperation.h>
#include <Authorize.h>
#include <BootNotification.h>
#include <Heartbeat.h>
#include <TargetValues.h>
#include <StartTransaction.h>
#include <StopTransaction.h>

ESP8266WiFiMulti WiFiMulti;

WebSocketsClient webSocket;

SmartChargingService *smartChargingService;
ChargePointStatusService *chargePointStatusService;
MeteringService *meteringService;


#ifndef STASSID
#define STASSID "YOUR_WLAN_SSID"
#define STAPSK  "YOUR_WLAN_PSWD"
#endif

//for demo tests in loop()
time_t testBegin;
bool test1Entered = true;
bool test2Entered = true;
bool test3Entered = true;
bool test4Entered = true;
bool test5Entered = true;
uint16_t loop_cnt = 0;
int num_test_cycle = 100;
bool halted = false;

const char* ssid     = STASSID;
const char* password = STAPSK;

#define ECHO_TEST true //True if tested against echo test server. Client connects to ECHO_HOST, ECHO_PORT and ECHO_URL. False if tested against real server. Client connects to WS_HOST, WS_PORT and WS_URL
#define WS_HOST "192.xxx.xxx.xxx"
#define WS_PORT 81
#define WS_URL "ws://192.xxx.xxx.xxx"
#define ECHO_HOST "wss://echo.websocket.org"
#define ECHO_PORT 80
#define ECHO_URL "wss://echo.websocket.org/"

/*
   Called by Websocket library on incoming message on the internet link
*/
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WSc] Disconnected!\n");
      break;
    case WStype_CONNECTED:
      Serial.printf("[WSc] Connected to url: %s\n", payload);
      break;
    case WStype_TEXT:
      if (DEBUG_APP_LAY) Serial.printf("[WSc] get text: %s\n", payload);

      if (!processWebSocketEvent(payload, length)) { //forward message to OcppEngine
        Serial.print(F("[WSc] Processing WebSocket input event failed!\n"));
      }
      break;
    case WStype_BIN:
      Serial.print(F("[WSc] Incoming binary data stream not supported"));
      break;
    case WStype_PING:
      // pong will be send automatically
      Serial.printf("[WSc] get ping\n");
      break;
    case WStype_PONG:
      // answer to a ping we send
      Serial.printf("[WSc] get pong\n");
      break;
  }
}



void setup() {
  Serial.begin(115200);

  Serial.setDebugOutput(true);

  Serial.println();
  Serial.println();
  Serial.println();

  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] BOOT WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }

  WiFiMulti.addAP(ssid, password);

  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
  }

  // server address, port and URL
  if (ECHO_TEST) {
    webSocket.begin(ECHO_HOST, ECHO_PORT, ECHO_URL);
  } else {
    webSocket.begin(WS_HOST, WS_PORT, WS_URL);
  }

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
  if (ECHO_TEST) webSocket.enableHeartbeat(15000, 3000, 2); //comment this one out to for specific OCPP servers

  /**
     The _real_ client code + test
  */
  ocppEngine_initialize(&webSocket, 2048); //default JSON document size = 2048
  smartChargingService = new SmartChargingService(16.0f); //default charging limit: 16A
  smartChargingService->setOnLimitChange([](float limit) {
    if (DEBUG_APP_LAY) Serial.print(F("[SmartChargingService] Limit Change arrived at Callback function: new limit = "));
    if (DEBUG_APP_LAY) Serial.print(limit);
    if (DEBUG_APP_LAY) Serial.print(F(" A\n"));
    EVSE_setChargingLimit(limit);
  });

  chargePointStatusService = new ChargePointStatusService(&webSocket); //adds itself to ocppEngine in constructor

  meteringService = new MeteringService(&webSocket);

  //set system time for test runs; will be without effect when tested against real server
  setTimeFromJsonDateString("2019-11-01T11:59:55.123Z");

  // BootNotification for automated testing. Comment out if used with a real CP
  OcppOperation *bootNotification = new BootNotification(&webSocket);
  initiateOcppOperation(bootNotification);
  bootNotification->setOnCompleteListener([](JsonDocument * json) {
    //start tests
    test1Entered = false;
    test2Entered = false;
    test3Entered = false;
    test4Entered = false;
    test5Entered = false;

    getTimeFromJsonDateString((*json)[2]["currentTime"], &testBegin);
  });

  // Proper BootNotification initiation for setup with real CP
  EVSE_setOnBoot([]() {
    OcppOperation *bootNotification = new BootNotification(&webSocket);
    initiateOcppOperation(bootNotification);
    bootNotification->setOnCompleteListener([](JsonDocument * json) {
      getTimeFromJsonDateString((*json)[2]["currentTime"], &testBegin);

      // Do custom stuff here. JsonDocument *json is the payload of the original response from the server.
    });
  });

  EVSE_setOnEvPlug([] () {
    OcppOperation *authorize = new Authorize(&webSocket);
    initiateOcppOperation(authorize);
    authorize->setOnCompleteListener([](JsonDocument * json) {
      if (DEBUG_APP_LAY) Serial.printf("Authorize answer just arrived at Callback function\n");

      // Do custom stuff here. In my test setup, there wasn't anything to do between an Authorize and StartTransaction
      // operation, so latter operation is chained by inserting it into the Authorize callback function.

      OcppOperation *startTransaction = new StartTransaction(&webSocket);
      initiateOcppOperation(startTransaction);
      startTransaction->setOnCompleteListener([](JsonDocument * json) {
        if (DEBUG_APP_LAY) Serial.printf("StartTransaction answer just arrived at Callback function\n");

        // Do custom stuff here. JsonDocument *json is the payload of the original response from the server.
      });
    });
  });

  EVSE_setOnEvUnplug([] () {
    OcppOperation *stopTransaction = new StopTransaction(&webSocket);
    initiateOcppOperation(stopTransaction);
    stopTransaction->setOnCompleteListener([](JsonDocument * json) {
      if (DEBUG_APP_LAY) Serial.printf("StopTransaction answer just arrived at Callback function\n\n");
    });
  });

  // comment out when testing in simulated setup
  //  meteringService->setPowerSampler([]() {
  //    return EVSE_readChargeRate();
  //  });
  //

  //TODO fully integrate in ChargePointStatusService

  EVSE_initialize();
}

void loop() {
  EVSE_loop();                        //necessary if the implementation of EVSE.h demands it
  webSocket.loop();                   //mandatory
  ocppEngine_loop();                  //mandatory
  smartChargingService->loop();       //optional
  chargePointStatusService->loop();   //optional
  meteringService->loop();            //optional

  /*
     Only the 6 lines above are important for operation with a real CP. The purpose of the following code
     is only automated testing. Just ignore it.
  */

  if (halted) return;

  loop_cnt++;

  time_t tTest = now();

  //const char* csProfile1 = "[2,1007,\"SetChargingProfile\",{\"connectorId\":1,\"csChargingProfiles\":{\"chargingProfileId\":107,\"stackLevel\":0,\"chargingProfilePurpose\":\"TxProfile\",\"chargingProfileKind\":\"Absolute\",\"validFrom\":\"2019-01-01T00:00:00.000Z\",\"validTo\":\"2022-01-01T00:00:00.000Z\",\"chargingSchedule\":{\"duration\":30,\"startSchedule\":\"2019-12-01T12:00:30.000Z\",\"chargingRateUnit\":\"A\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":32,\"numberPhases\":3},{\"startPeriod\":15,\"limit\":0,\"numberPhases\":3}]}}}]";
  //const char* csProfile2 = "[2,1008,\"SetChargingProfile\",{\"connectorId\":1,\"csChargingProfiles\":{\"chargingProfileId\":108,\"stackLevel\":1,\"chargingProfilePurpose\":\"TxProfile\",\"chargingProfileKind\":\"Absolute\",\"validFrom\":\"2019-01-01T00:00:00.000Z\",\"validTo\":\"2022-01-01T00:00:00.000Z\",\"chargingSchedule\":{\"duration\":30,\"startSchedule\":\"2019-12-01T12:00:59.000Z\",\"chargingRateUnit\":\"A\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":8,\"numberPhases\":3},{\"startPeriod\":15,\"limit\":32,\"numberPhases\":3}]}}}]";

  /*
     Mark the begin of the automated test routine on the console output. One run of the routine consists of plugging the charge cable
     and therefore initiating a charging session and then unplugging the cable again and ending the charging session.
     The routine is repeated 100 times.
  */
  if (tTest - testBegin >= 0UL && !test2Entered) { //After 0s, execute once
    Serial.print(F("\n\n    - Start of test routine -\n\n"));
    test2Entered = true;
  }

  /*
     Test 1: User plugs in EV
  */
  if (tTest - testBegin >= 3 && !test1Entered) { //After 3s, execute once

    OcppOperation *authorize = new Authorize(&webSocket);
    initiateOcppOperation(authorize);
    authorize->setOnCompleteListener([](JsonDocument * json) {
      Serial.printf("Authorize answer just arrived at Callback function\n");

      OcppOperation *startTransaction = new StartTransaction(&webSocket);
      initiateOcppOperation(startTransaction);
      startTransaction->setOnCompleteListener([](JsonDocument * json) {
        Serial.printf("StartTransaction answer just arrived at Callback function\n");
      });
    });

    test1Entered = true;
  }

  /*
     Test 2: Central system sets Smart Charging Profiles

     ONLY WORKS FOR TESTING AGAINST ECHO SERVERS
  */
  if (ECHO_TEST && tTest - testBegin >= 15 && !test2Entered) { //After 15s, execute once
    if (DEBUG_APP_LAY) Serial.print(F("[Test2] after 00-15: Set Smart Charging Profiles (emulated by Esp8266)\n"));

    const char* csProfile1 = "[2,1000,\"SetChargingProfile\",{\"connectorId\":1,\"csChargingProfiles\":{\"chargingProfileId\":100,\"stackLevel\":0,\"chargingProfilePurpose\":\"TxDefaultProfile\",\"chargingProfileKind\":\"Recurring\",\"recurrencyKind\":\"Daily\",\"chargingSchedule\":{\"duration\":86400,\"startSchedule\":\"2019-01-01T00:00:00.000Z\",\"chargingRateUnit\":\"A\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":16,\"numberPhases\":3},{\"startPeriod\":28800,\"limit\":8,\"numberPhases\":3},{\"startPeriod\":72000,\"limit\":16,\"numberPhases\":3}]}}}]";
    //const char* csProfile2 = "[2,1001,\"SetChargingProfile\",{\"connectorId\":1,\"csChargingProfiles\":{\"chargingProfileId\":101,\"stackLevel\":0,\"chargingProfilePurpose\":\"TxProfile\",\"chargingProfileKind\":\"Relative\",\"validFrom\":\"2019-01-01T00:00:00.000Z\",\"validTo\":\"2022-01-01T00:00:00.000Z\",\"chargingSchedule\":{\"duration\":120,\"chargingRateUnit\":\"A\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":16,\"numberPhases\":3},{\"startPeriod\":10,\"limit\":24,\"numberPhases\":3},{\"startPeriod\":20,\"limit\":32,\"numberPhases\":3},{\"startPeriod\":40,\"limit\":24,\"numberPhases\":3},{\"startPeriod\":50,\"limit\":16,\"numberPhases\":3},{\"startPeriod\":60,\"limit\":8,\"numberPhases\":3}]}}}]";
    //const char* csProfile3 = "[2,1002,\"SetChargingProfile\",{\"connectorId\":1,\"csChargingProfiles\":{\"chargingProfileId\":102,\"stackLevel\":10,\"chargingProfilePurpose\":\"TxProfile\",\"chargingProfileKind\":\"Absolute\",\"validFrom\":\"2019-01-01T00:00:00.000Z\",\"validTo\":\"2022-01-01T00:00:00.000Z\",\"chargingSchedule\":{\"duration\":20,\"startSchedule\":\"2019-12-01T12:01:00.000Z\",\"chargingRateUnit\":\"A\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":32,\"numberPhases\":3}]}}}]";
    const char* csProfile3 = "[2,1002,\"SetChargingProfile\",{\"connectorId\":1,\"csChargingProfiles\":{\"chargingProfileId\":102,\"stackLevel\":10,\"chargingProfilePurpose\":\"TxProfile\",\"chargingProfileKind\":\"Absolute\",\"validFrom\":\"2019-01-01T00:00:00.000Z\",\"validTo\":\"2022-01-01T00:00:00.000Z\",\"chargingSchedule\":{\"duration\":60,\"startSchedule\":\"2019-12-01T12:01:00.000Z\",\"chargingRateUnit\":\"A\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":32,\"numberPhases\":3},{\"startPeriod\":30,\"limit\":0,\"numberPhases\":3}]}}}]";

    const char* csProfile2 = "[2,105,\"SetChargingProfile\",{\"connectorId\":1,\"csChargingProfiles\":{\"chargingProfileId\":105,\"stackLevel\":0,\"chargingProfilePurpose\":\"TxProfile\",\"chargingProfileKind\":\"Relative\",\"validFrom\":\"2019-01-01T00:00:00.000Z\",\"validTo\":\"2022-01-01T00:00:00.000Z\",\"chargingSchedule\":{\"duration\":120,\"chargingRateUnit\":\"A\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":8,\"numberPhases\":3},{\"startPeriod\":10,\"limit\":16,\"numberPhases\":3},{\"startPeriod\":20,\"limit\":24,\"numberPhases\":3},{\"startPeriod\":30,\"limit\":32,\"numberPhases\":3},{\"startPeriod\":40,\"limit\":0,\"numberPhases\":3},{\"startPeriod\":50,\"limit\":16,\"numberPhases\":3}]}}}]";


    webSocket.sendTXT(csProfile1, strlen(csProfile1));
    webSocket.sendTXT(csProfile2, strlen(csProfile1));
    webSocket.sendTXT(csProfile3, strlen(csProfile1));

    Serial.print(F("[Test2] test fully executed\n"));

    test2Entered = true;
  }

  /*
     Test 3: Charging station sends a heartbeat request
  */
  if (tTest - testBegin >= 1 && !test3Entered) {  //After 1s, execute once
    OcppOperation *heartbeat = new Heartbeat(&webSocket);
    initiateOcppOperation(heartbeat);
    heartbeat->setOnCompleteListener([](JsonDocument * json) {
      Serial.printf("Heartbeat answer just at Callback function\n\n");
    });

    test3Entered = true;
  }

  /*
     Test 4: User unplugs EV
  */
  if (tTest - testBegin >= 6 && !test4Entered) { //After 6s, execute once
    OcppOperation *stopTransaction = new StopTransaction(&webSocket);
    initiateOcppOperation(stopTransaction);
    stopTransaction->setOnCompleteListener([](JsonDocument * json) {
      Serial.printf("StopTransaction answer just at Callback function\n\n");
    });

    test4Entered = true;
  }

  /*
     Start routine over again.
  */
  if (tTest - testBegin >= 8 && !test5Entered) { //After 8s, execute once

    num_test_cycle--;
    if (num_test_cycle > 0) {
      testBegin = tTest; //align testBegin to tTest, so the defined times for the test actions fit for the next run

      test1Entered = false;
      test2Entered = false;
      test3Entered = false;
      test4Entered = false;
      test5Entered = false;
    } else {
      Serial.print(F("\n\n    - All test routines executed -\n"));
      halted = true;
    }
  }
}