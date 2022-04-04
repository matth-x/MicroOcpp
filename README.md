# <img src="https://user-images.githubusercontent.com/63792403/133922028-fefc8abb-fde9-460b-826f-09a458502d17.png" alt="Icon" height="24"> &nbsp; ArduinoOcpp
OCPP-J 1.6 client for the ESP8266 and the ESP32 (more coming soon)

Reference usage: [OpenEVSE](https://github.com/OpenEVSE/ESP32_WiFi_V4.x/blob/master/src/ocpp.cpp)

PlatformIO package: [ArduinoOcpp](https://platformio.org/lib/show/11975/ArduinoOcpp)

Website: [www.arduino-ocpp.com](https://www.arduino-ocpp.com)

Full compatibility with the Arduino platform. Need a **FreeRTOS** version? Please [contact me](https://github.com/matth-x/ArduinoOcpp#further-help)

## Make your EVSE ready for OCPP :car::electric_plug::battery:

You can build an OCPP Charge Point controller using the popular, Wi-Fi enabled microcontrollers ESP8266, ESP32 and comparable. This library allows your EVSE to communicate with an OCPP Central System and to participate in your Charging Network.

:heavy_check_mark: Works with [SteVe](https://github.com/RWTH-i5-IDSG/steve) and [The Mobility House OCPP package](https://github.com/mobilityhouse/ocpp)

:heavy_check_mark: Passed compatibility tests with further commercial Central Systems

:heavy_check_mark: Integrated and tested in many charging stations

### Features

- handles the OCPP communication with the charging network
- implements the standard OCPP charging process
- provides an API for the integration of the hardware modules of your EVSE
- works with any TLS implementation and WebSocket library. E.g.
   - Arduino networking stack: [Links2004/arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets), or
   - generic embedded systems: [Mongoose Networking Library](https://github.com/cesanta/mongoose)

For simple chargers, the necessary hardware and internet integration is usually far below 1000 LOCs.

## Usage guide

**This feature branch is WIP. A usage guide will follow.**

## Supported operations

| Operation name | supported | in progress | not supported |
| -------------- | :---------: | :-----------: | :-------------: |
| **Core profile** |
| `Authorize` | :heavy_check_mark: |
| `BootNotification` | :heavy_check_mark: |
| `ChangeAvailability` | :heavy_check_mark: |
| `ChangeConfiguration` | :heavy_check_mark: |
| `ClearCache` | :heavy_check_mark: |
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

- [x] introduce proper offline behavior and package loss / fault detection
- [x] handle fragmented input messages correctly
- [x] add support for multiple power connectors
- [x] add support for the ESP32
- [ ] reach full compliance to OCPP 1.6 Smart Charging Profile
- [ ] integrate Authorization Cache
- [ ] **get ready for OCPP 2.0.1**

## Further help

I hope this guide can help you to successfully integrate an OCPP controller into your EVSE. If something needs clarification or if you have a question, please send me a message.

:envelope: : matthias A⊤ arduino-ocpp DО⊤ com

If you want professional assistance for your EVSE project, you can contact me as well. I'm looking forward to hearing about your ideas!
