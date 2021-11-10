# <img src="https://user-images.githubusercontent.com/63792403/133922028-fefc8abb-fde9-460b-826f-09a458502d17.png" alt="Icon" height="24"> &nbsp; ArduinoOcpp
OCPP-J 1.6 client for the ESP8266 and the ESP32 (more coming soon)

Reference usage: [OpenEVSE](https://github.com/OpenEVSE/ESP32_WiFi_V4.x/blob/master/src/ocpp.cpp)

PlatformIO package: [ArduinoOcpp](https://platformio.org/lib/show/11975/ArduinoOcpp)

Website: [www.arduino-ocpp.com](https://www.arduino-ocpp.com)

Full compatibility with the Arduino platform. Need a **FreeRTOS** version? Please [contact me](https://github.com/matth-x/ArduinoOcpp#further-help)

## Make your EVSE ready for OCPP :car::electric_plug::battery:

You can build an OCPP Charge Point controller using the popular, Wi-Fi enabled microcontrollers ESP8266, ESP32 and comparable. This library allows your EVSE to communicate with an OCPP Central System and to participate in your Charging Network.

:heavy_check_mark: Works with [SteVe](https://github.com/RWTH-i5-IDSG/steve)

:heavy_check_mark: Tested with two further (proprietary) central systems

:heavy_check_mark: Integrated and tested in many charging stations (including a ClipperCreek Inc. station)

### Features

- lets you initiate all supported OCPP operations (see the table at the bottom of this page)
- responds to requests from the central system and notifies your client code by listeners
- manages the EVSE data model as specified by OCPP and does a lot of the paperwork. For example, it sends `StatusNotification` or `MeterValues` messages by itself.

You still have the responsibility (or freedom) to design the application logic of the charger and to integrate the HW components. This library doesn't

- define physical reactions on the messages from the central system (CS). For example, when you initiate a `StartTransaction` request which the CS accepts, the library stores the new EVSE status including the `transactionId`, but lets you define which action to take.

For simple chargers, the application logic + HW integration is far below 1000 LOCs.

## Usage guide

Please take `OneConnector_EVSE.ino` (in the `examples/OneConnector-EVSE/` folder) as starting point for you first project. It is an integration of a simple GPIO-based charger with one connector. See `OneConnector_HW_integration.ino` in which the charger functions are mapped onto the OCPP library to get a feeling for how to use this library in practice. In this guide, I give a brief overview of the key concepts.

- To get the library running, you have to install all dependencies (see the list below).

   - In case you use PlatformIO, you can just add `matth-x/ArduinoOcpp` to your project using the PIO library manager.

- In your project's `main` file, include `ArduinoOcpp.h`. This gives you a simple access to all functions.

- Before establishing an OCPP connection you have to ensure that your device has access to a Wi-Fi access point. All debug messages are printed on the standard serial (i.e. `Serial.print(F("debug msg"))`).

- To connect to your OCPP Central System, call `OCPP_initialize(String OCPP_HOST, uint16_t OCPP_PORT, String OCPP_URL)`. You need to insert the address parameters according to the configuration of your central system. Internally, the library passes these parameters to the WebSocket object without further alteration.

- In your `setup()` function, you can add the configuration functions from `ArduinoOcpp.h` to properly integrate your hardware. All configuration functions are documented in `ArduinoOcpp.h`. For example, to integrate the energy meter of your EVSE, add

```cpp
setEnergyActiveImportSampler([]() {
    return yourEVSE_readEnergyMeter();
});
```

- Add `OCPP_loop()` to your `loop()` function.

- There are a couple of OCPP operations you can initialize on your EVSE. For example, to send a `Boot Notification`, use the function 
```cpp
void bootNotification(String chargePointModel, String chargePointVendor, OnReceiveConfListener onConf = NULL, ...)`
```

In practice, it looks like this:

```cpp
void setup() {

    ... //other code including the initialization of Wi-Fi and OCPP

    bootNotification("GPIO-based CP model", "Greatest EVSE vendor", [] (JsonObject confMsg) {
        //This callback is executed when the .conf() response from the central system arrives
        Serial.print(F("BootNotification was answered. Central System clock: "));
        Serial.println(confMsg["currentTime"].as<String>());
    
        evseIsBooted = true; //notify your hardware that the BootNotification.conf() has arrived
    });
    
    ... //rest of setup() function; executed immediately as bootNotification() is non-blocking
}
```

The parameters `chargePointModel` and `chargePointVendor` are equivalent to the parameters in the `Boot Notification` as defined by the OCPP specification. The last parameter `OnReceiveConfListener onConf` is a callback function which the library executes when the central system has processed the operation and the ESP has received the `.conf()` response. Here you can add your device-specific behavior, e.g. flash a confirmation LED or unlock the connectors. If you don't need it, the last parameter is optional.

For your first EVSE integration, the `onReceiveConfListener` is probably sufficient. For advanced EVSE projects, the other listeners likely become relevant:
  
- `onAbortListener`: will be called whenever the engine stops trying to finish an operation normally which was initiated by this device.

- `onTimeoutListener`: will be executed when the operation is not answered until the timeout expires. Note that timeouts also trigger the `onAbortListener`.

- `onReceiveErrorListener`: will be called when the Central System returns a CallError. Again, each error also triggers the `onAbortListener`.
  
Following example shows the correct usage of all listeners.


```cpp
authorize(idTag, [](JsonObject conf) { 
    //onReceiveConfListener (optional but very likely necessary for your integration)
    successfullyAuthorized = true; //example client code
    ... //further client code, e.g. call startTransaction()
}, []() { 
    //onAbortListener (optional)
    Serial.print(F("[EVSE] Could not authorize charging session! Aborted\n")); //flash error light etc.
}, []() { 
    //onTimeoutListener (optional)
    Serial.print(F("[EVSE] Could not authorize charging session! Reason: timeout\n"));
}, [](const char *code, const char *description, JsonObject details) { 
    //onReceiveErrorListener (optional)
    Serial.print(F("[EVSE] Could not authorize charging session! Reason: received OCPP error: "));
    Serial.println(code);
});

```

The library also reacts on CS-initiated operations. You can add your own behavior there too. For example, when you want to flash a LED on receipt of a `Set Charging Profile` request, use the following function.

```cpp
void setOnSetChargingProfileRequest(void listener(JsonObject payload));
```

You can also process the original payload from the CS using the `payload` object.

To get started quickly with or without EVSE hardware, you can flash the sketch in `examples/SECC` onto your ESP. That example mimics a full OCPP communications controller as it would look like in a real charging station. You can build a charger prototype based on that example or just view the internal state using the device monitor.

## Dependencies

- [bblanchon/ArduinoJSON](https://github.com/bblanchon/ArduinoJson)
- [Links2004/arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets)

In case you use PlatformIO, you can copy all dependencies from `platformio.ini` into your own configuration file. Alternatively, you can install the full library with dependencies by adding `matth-x/ArduinoOcpp` in the PIO library manager.

## Supported operations

| Operation name | supported | in progress | not supported |
| -------------- | :---------: | :-----------: | :-------------: |
| **Core profile** |
| `Authorize` | :heavy_check_mark: |
| `BootNotification` | :heavy_check_mark: |
| `ChangeAvailability` | :heavy_check_mark: |
| `ChangeConfiguration` | :heavy_check_mark: |
| `ClearCache` |   |   | :heavy_multiplication_x: |
| `DataTransfer` | :heavy_check_mark: |
| `GetConfiguration` | :heavy_check_mark: |
| `Heartbeat` | :heavy_check_mark: |
| `MeterValues` | :heavy_check_mark: |
| `RemoteStartTransaction` | :heavy_check_mark: |
| `RemoteStopTransaction` | :heavy_check_mark: |
| `Reset` | :heavy_check_mark: |
| `StartTransaction` | :heavy_check_mark: |
| `StatusNotification` | :heavy_check_mark: |
| `StopTransaction` | :heavy_check_mark: |
| `UnlockConnector` | :heavy_check_mark: |
| **Smart charging profile** |
| `ClearChargingProfile` | :heavy_check_mark: |
| `GetCompositeSchedule` |   |   | :heavy_multiplication_x: |
| `SetChargingProfile` | :heavy_check_mark: |
| **Remote trigger profile** |
| `TriggerMessage` | :heavy_check_mark: |
| **Firmware management** |
| `GetDiagnostics` | :heavy_check_mark: |
| `DiagnosticsStatusNotification` | :heavy_check_mark: |
| `FirmwareStatusNotification` | :heavy_check_mark: |
| `UpdateFirmware` | :heavy_check_mark: |

## Next development steps

- [x] introduce a timeout mechanism
- [x] some refactoring steps (e.g. separate RPC header from OCPP payload creation)
- [x] add facade for rapid integration
- [x] introduce proper offline behavior and package loss / fault detection
- [x] handle fragmented input messages correctly
- [x] add support for multiple power connectors
- [x] add support for the ESP32
- [ ] reach full compliance to OCPP 1.6 Smart Charging Profile
- [ ] **get ready for OCPP 2.0.1**

## Further help

I hope this guide can help you to successfully integrate an OCPP controller into your EVSE. If something needs clarification or if you have a question, please send me a message.

:envelope: : matthias A⊤ arduino-ocpp DО⊤ com

If you want professional assistance for your EVSE project, you can contact me as well. I'm looking forward to hearing about your ideas!
