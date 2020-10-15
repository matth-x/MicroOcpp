# ESP8266-OCPP
OCPP 1.6 Smart Charging client for the ESP8266

## Get the Smart Grid on your Home Charger :car::electric_plug::battery:

You can easily turn your ESP8266 into an OCPP charge point controller. This library provides the tools for the web communication part of your project.

:heavy_check_mark: Works with [SteVe](https://github.com/RWTH-i5-IDSG/steve)

:heavy_check_mark: Tested with two further (proprietary) central systems

:heavy_check_mark: Already integrated in two charging stations (including a ClipperCreek Inc. station)

### Features

- lets you initiate all supported OCPP operations
- responds to requests from the central system and notifies your client code by listeners
- manages the EVSE data model as specified by OCPP and does a lot of the paperwork. For example, it sends `StatusNotification` or `MeterValues` messages by itself.

You still have the responsibility (or freedom) to design the application logic of the charger and to integrate the HW components. This library doesn't

- define physical reactions on the messages from the central system (CS). For example, when you initiate an Authorize request which the CS accepts, the library stores the new EVSE status but lets you define which action to take.

For simple chargers, the application logic + HW integration is far below 1000 LOCs.

## Usage guide

Please take `OneConnector_EVSE.ino` (in the `examples/OneConnector-EVSE/` folder) as starting point for you first project. It is an integration of a simple GPIO-based charger with one connector. See `OneConnector_HW_integration.ino` in which the charger functions are mapped onto the OCPP library to get a feeling for how to use this library in practice. In this guide, I give a brief overview of the key concepts.

- To get the library running, you have to install all dependencies (see the list below).

- In your project's `main` file, include `SingleConnectorEvseFacade.h`. This gives you a simple access to all functions.

- Before establishing an OCPP connection you have to ensure that your device has access to a Wi-Fi access point. All debug messages are printed on the standard serial (i.e. `Serial.print(F("debug msg"))`).

- To create the OCPP connection, call `OCPP_initialize(String OCPP_HOST, uint16_t OCPP_PORT, String OCPP_URL)`. You need to insert the parameters according to the configuration of your central system. Internally, the library uses these parameters without further alteration to establish a WebSocket connection.

- In your `setup()` function, you can add the configuration functions from `SingleConnectorEvseFacade.h` to properly integrate your hardware. All configuration functions are documented in `SingleConnectorEvseFacade.h`. For example, to integrate the energy meter of your EVSE, add

```cpp
setEnergyActiveImportSampler([]() {
    return yourEVSE_readEnergyMeter();
});
```

- Add `OCPP_loop()` to your `loop()` function.

- There are a couple of OCPP operations you can already use. For example, to send a `Boot Notification`, use the function 
```cpp
void bootNotification(String chargePointModel, String chargePointVendor, OnReceiveConfListener onConf)`
```

In practice, it looks like this:

```cpp
bootNotification("Greatest EVSE vendor", "GPIO-based CP model", [] (JsonObject confMsg) {
    //This callback is executed when the .conf() response from the central system arrives
    Serial.print(F("BootNotification was answered at "));
    Serial.print(confMsg["currentTime"].as<String>());
    Serial.print(F("\n"));

    evseIsBooted = true; //notify your hardware that the BootNotification.conf() has arrived
});
```

The parameters `chargePointModel` and `chargePointVendor` are equivalent to the parameters in the `Boot Notification` as defined by the OCPP specification. The last parameter `OnReceiveConfListener onConf` is a callback function which the library executes when the central system has processed the operation and the ESP has received the `.conf()` response. Here you can add your device-specific behavior, e.g. flash a confirmation LED or unlock the connectors. If you don't need it, the last parameter is optional.

- The library also reacts on CS-initiated operations. You can add your own behavior there too. For example, when you want to flash a LED on receipt of a `Set Charging Profile` request, use the following function.

```cpp
void setOnSetChargingProfileRequest(void listener(JsonObject payload));
```

You can also process the original payload from the CS using the `payload` object.

To get started quickly without EVSE hardware, you can use the charge point simulation in `examples/Simulation_without_HW/` as a starting point. It establishes a connection to a WebSocket echo server (`wss://echo.websocket.org/`). This works because the library can answer its own requests to make testing easier. You can also run the simulation against the URL of your central system. `Simulated_HW.ino` simulates the user interaction at the EVSE. It repeats its test routine over and over again. You can watch it in your device monitor! 

## Dependencies

- bblanchon/ArduinoJSON
- Links2004/arduinoWebSockets
- ivanseidel/LinkedList
- PaulStoffregen/Time

## Next development steps

- [x] introduce a timeout mechanism
- [x] some refactoring steps (e.g. separate RPC header from OCPP payload creation)
- [x] add facade for rapid integration
- [ ] introduce proper offline behavior and package loss / fault detection
- [ ] handle fragmented input messages correctly
- [x] add support for multiple power connectors
- [ ] reach full compliance to OCPP 1.6 Smart Charging Profile
- [ ] **get ready for OCPP 2.0.1**
