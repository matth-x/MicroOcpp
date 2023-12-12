# <img src="https://github.com/matth-x/MicroOcpp/assets/63792403/1c49d1ad-7afc-48d3-a54e-9aef2d4886db" alt="Icon" height="24"> &nbsp; MicroOcpp

[![Build Status]( https://github.com/matth-x/MicroOcpp/workflows/PlatformIO%20CI/badge.svg)](https://github.com/matth-x/MicroOcpp/actions)
[![Unit tests]( https://github.com/matth-x/MicroOcpp/workflows/Unit%20tests/badge.svg)](https://github.com/matth-x/MicroOcpp/actions)
[![codecov](https://codecov.io/github/matth-x/ArduinoOcpp/branch/develop/graph/badge.svg?token=UN6LO96HM7)](https://codecov.io/github/matth-x/ArduinoOcpp)

OCPP 1.6 client for microcontrollers. Portable C/C++. Compatible with Espressif, Arduino, NXP, STM, Linux and more.

**Formerly ArduinoOcpp ([migration guide](https://matth-x.github.io/MicroOcpp/migration/))**: *the initial version of this library used the Arduino API but this dependency was dropped some time ago and the old name has become outdated. Despite the new name, nothing changes for existing users and the Arduino integration will continue to be fully functional.*

Reference usage: [OpenEVSE](https://github.com/OpenEVSE/ESP32_WiFi_V4.x/blob/master/src/ocpp.cpp) | Technical introduction: [Docs](https://matth-x.github.io/MicroOcpp/intro-tech) | Website: [www.micro-ocpp.com](https://www.micro-ocpp.com)

:heavy_check_mark: Works with [SteVe](https://github.com/RWTH-i5-IDSG/steve) and [15+ commercial Central Systems](https://www.micro-ocpp.com/#h.314525e8447cc93c_81)

:heavy_check_mark: Eligible for public chargers (Eichrecht-compliant)

:heavy_check_mark: Supports all OCPP 1.6 feature profiles

## Tester / Demo App

*Main repository: [MicroOcppSimulator](https://github.com/matth-x/MicroOcppSimulator)*

(Beta) The Simulator is a demo & development tool for MicroOcpp which allows to quickly assess the compatibility with different OCPP backends. It simulates a full charging station, adds a GUI and a mocked hardware binding to MicroOcpp and runs in the browser (using WebAssembly): [Try it](https://demo.micro-ocpp.com/)

<div align="center"><img src="https://github.com/matth-x/MicroOcpp/assets/63792403/27f2819b-41fd-41a7-88a8-9e673b8a88b8" alt="Screenshot" width="800em" href="https://demo.micro-ocpp.com/"></div>

#### Usage

**OCPP server setup**: Navigate to "Control Center". In the WebSocket options, add the OCPP backend URL, charge box ID and authorization key if existent. Press "Update WebSocket" to save. The Simulator should connect to the OCPP server. To check the connection status, it could be helpful to open the developer tools of the browser.

If you don't have an OCPP server at hand, leave the charge box ID blank and enter the following backend address: `wss://echo.websocket.events/` (this server is sponsored by Lob.com)

**RFID authentication**: Go to "Control Center" > "Connectors" > "Transaction" and update the idTag with the desired value.

## Benchmarks

*Full report: [MicroOcpp benchmark (esp-idf)](https://github.com/matth-x/MicroOcpp-benchmark)*

The following measurements were taken on the ESP32 @ 160MHz and represent the optimistic best case scenario for a charger with two physical connectors (i.e. compiled with `-Os`, disabled debug output and logs).

| Description | Value |
| :--- | ---: |
| Flash size (minimal) | 121,170 B |
| Heap occupation (idle) | 12,308 B |
| Heap occupation (peak) | 21,916 B |
| Initailization | 21 ms |
| `loop()` call (idle) | 0.05 ms |
| Large message sent | 5 ms |

In practical setups, the execution time is largely determined by IO delays and the heap occupation is significantly influenced by the configuration with reservation, local authorization and charging profile lists.

## Developers guide

PlatformIO package: [MicroOcpp](https://registry.platformio.org/libraries/matth-x/MicroOcpp)

MicroOcpp is an implementation of the OCPP communication behavior. It automatically initiates the corresponding OCPP operations once the hardware status changes or the RFID input is updated with a new value. Conversely it processes new data from the server, stores it locally and updates the hardware controls when applicable.

Please take `examples/ESP/main.cpp` as the starting point for the first project. It is a minimal example which shows how to establish an OCPP connection and how to start and stop charging sessions. The API documentation can be found in [`MicroOcpp.h`](https://github.com/matth-x/MicroOcpp/blob/master/src/MicroOcpp.h). Also check out the [Docs](https://matth-x.github.io/MicroOcpp).

### Dependencies

Mandatory:

- [bblanchon/ArduinoJSON](https://github.com/bblanchon/ArduinoJson)

If compiled with the Arduino integration:

- [Links2004/arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets) (version `2.4.1`)

In case you use PlatformIO, you can copy all dependencies from `platformio.ini` into your own configuration file. Alternatively, you can install the full library with dependencies by adding `matth-x/MicroOcpp@1.0.0` in the PIO library manager.

## OCPP 2.0.1 and ISO 15118

MicroOcpp will be upgraded to OCPP 2.0.1 soon. The API has already been prepared for transitioning between both versions, so an integration of the current library version will also be functional with the 2.0.1 upgrade.

ISO 15118 defines some use cases which include a message exchange between the charger and server. This library facilitates the integration of ISO 15118 by handling its OCPP-side communication. A public demonstration will follow with the first collaboration on an open OCPP 2.0.1 + ISO 15118 integration.

## Contact details

I hope the given documentation and guidance can help you to successfully integrate an OCPP controller into your EVSE. I will be happy if you reach out!

:envelope: : matthias [A⊤] arduino-ocpp [DО⊤] com
