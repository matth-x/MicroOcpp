# ESP8266-OCPP
OCPP 1.6 Smart Charging client for the ESP8266

## Get the Smart Grid on your Home Charger :car::electric_plug::battery:

You can easily turn your ESP8266 into an OCPP charge point controller. This library provides the tools for the web communication part of your project.

:heavy_check_mark: Works with [SteVe](https://github.com/RWTH-i5-IDSG/steve)

:heavy_check_mark: Tested with two further (proprietary) central systems

:heavy_check_mark: Already integrated in two charging stations (including a ClipperCreek Inc. station)

### Features

- lets you initiate any kind of OCPP operation
- responds to requests from the central system and notifies your client code by listeners
- manages the EVSE data model as specified by OCPP and does a lot of the paperwork. For example, it sends `StatusNotification` or `MeterValues` messages by itself.

You still have the responsibility (or freedom) to design the application logic of the charger and to integrate the HW components. This library doesn't

- define reactions on the messages from the central system (CS). For example, when you initiate an Authorize request which the CS accepts, the library stores the new EVSE status but lets you define which action to take.

For simple chargers, the application logic + HW integration is far below 1000 LOCs.

## Usage guide

Please take the `example_client.ino` (in the examples folder) as starting point for your first project. I will keep it up to date and insert an demonstration for each new feature. Here is an overview of the key concepts:

- To simply send an OCPP operation (e.g. `BootNotification`), insert
```cpp
OcppOperation *bootNotification = makeOcppOperation(&webSocket,
    new BootNotification());
initiateOcppOperation(bootNotification);
```
- To send an OCPP operation and define an action when it succeeds, insert
```cpp
OcppOperation *bootNotification = makeOcppOperation(&webSocket,
    new BootNotification());
initiateOcppOperation(bootNotification);
bootNotification->setOnReceiveConfListener([](JsonObject payload) {
    Serial.print(F("BootNotification at client callback.\n"));
    // e.g. flash a confirmation light on your EVSE here
});
```
- To define an action when a requirement message comes in from the CS, insert in `setup()`
```cpp
setOnSetChargingProfileRequestListener([](JsonObject payload) {
    Serial.print(F("New charging profile was set!.\n"));
    // e.g. refresh your display. Note that the Smart Charging logic is already implemented in SmartChargingService.cpp
});
```
- To integrate your HW, you have to write a file that implements all functions from `EVSE.h`. For example:
```cpp
float EVSE_readChargeRate() {
    float chargeRate = myPowerMeter.getActivePowerInW();
    return chargeRate; //in W
}
```
Please take `EVSE_test_routines.ino` as an example.

The charge point simulation in the example folder establishes a connection to a WebSocket echo server (`wss://echo.websocket.org/`). You can also run the simulation against the URL of your central system. The `EVSE_test_routines.ino` implements all EVSE functionalities and simulates the user interaction at the EVSE. It repeats its test routine over and over again. You can watch it in your device monitor! 

## Dependencies

- bblanchon/ArduinoJSON
- Links2004/arduinoWebSockets
- ivanseidel/LinkedList
- PaulStoffregen/Time

## Next development steps

- [x] introduce a timeout mechanism
- [x] some refactoring steps (e.g. separate RPC header from OCPP payload creation)
- [ ] introduce proper offline behavior and package loss / fault detection
- [ ] handle fragmented input messages correctly
- [ ] add support for multiple power connectors
- [ ] reach full compliance to OCPP 1.6 Smart Charging Profile
- [ ] **get ready for OCPP 2.0.1**
