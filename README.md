# <img src="https://user-images.githubusercontent.com/63792403/133922028-fefc8abb-fde9-460b-826f-09a458502d17.png" alt="Icon" height="24"> &nbsp; MicroOcpp (ArduinoOcpp)

[![Build Status]( https://github.com/matth-x/MicroOcpp/workflows/PlatformIO%20CI/badge.svg)](https://github.com/matth-x/MicroOcpp/actions)
[![Unit tests]( https://github.com/matth-x/MicroOcpp/workflows/Unit%20tests/badge.svg)](https://github.com/matth-x/MicroOcpp/actions)
[![codecov](https://codecov.io/github/matth-x/ArduinoOcpp/branch/develop/graph/badge.svg?token=UN6LO96HM7)](https://codecov.io/github/matth-x/ArduinoOcpp)

**Formerly ArduinoOcpp ([migration guide](https://matth-x.github.io/MicroOcpp/migration/))**: *since the first release in 2020, ArduinoOcpp has increasingly been used on other platforms than Arduino. The goal of this project has evolved to fit seamlessly in a high variety of microcontroller firmwares and so the dependency on Arduino was dropped some time ago. The new name reflects the purpose of this project better and prevents potential confusion. Despite the new name, nothing changes for existing users and the Arduino integration will continue to be fully functional.*

OCPP-J 1.6 client for embedded microcontrollers. Portable C/C++. Compatible with Espressif, NXP, Texas Instruments and STM.

Reference usage: [OpenEVSE](https://github.com/OpenEVSE/ESP32_WiFi_V4.x/blob/master/src/ocpp.cpp)

Technical introduction: [MicroOcpp Docs](https://matth-x.github.io/MicroOcpp/intro-tech)

Try it (online): [MicroOcppSimulator](https://github.com/matth-x/MicroOcppSimulator)

Fully integrated into the Arduino platform and the ESP32 / ESP8266. Runs on ESP-IDF, FreeRTOS and generic embedded C/C++ platforms.

## Make your EVSE ready for OCPP :car::electric_plug::battery:

This library allows EVSEs to communicate with an OCPP Backend and to participate in commercial charging networks. You can integrate it into an existing firmware development, or start a new EVSE project from scratch with it.

:heavy_check_mark: Works with [SteVe](https://github.com/RWTH-i5-IDSG/steve) and [15+ commercial Central Systems](https://www.arduino-ocpp.com/#h.314525e8447cc93c_81)

:heavy_check_mark: Tested in many charging stations

:heavy_check_mark: Eligible for public chargers (Eichrecht-compliant)

Website: [www.arduino-ocpp.com](https://www.arduino-ocpp.com)

### Features

- handles the OCPP communication with the charging network
- implements the standard OCPP charging process
- provides an API for the integration of the hardware modules of the EVSE
- works with any TLS implementation and WebSocket library. E.g.
   - Arduino networking stack: [Links2004/arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets), or
   - generic embedded systems: [Mongoose Networking Library](https://github.com/cesanta/mongoose)

The necessary hardware and internet integration is usually far below 1000 LOCs.

## Demo App

*Main repository: [MicroOcppSimulator](https://github.com/matth-x/MicroOcppSimulator)*

(Beta) The Simulator is a demo & development tool which allows to quickly assess the compatibility of MicroOcpp with OCPP backends. It simulates a full charging station, adds a GUI and basic controls to MicroOcpp and runs in the browser (using WebAssembly): [Try it](https://demo.micro-ocpp.com/)

[![Screenshot](https://github.com/agruenb/arduino-ocpp-dashboard/blob/master/docs/img/status_page.png)](https://demo.micro-ocpp.com/)

### Usage

**OCPP server setup**: Navigate to "Control Center". In the WebSocket options, add the OCPP backend URL, charge box ID and authorization key if existent. Press "Update WebSocket" to save. The Simulator should connect to the OCPP server. To check the connection status, it could be helpful to open the developer tools of the browser.

If you don't have an OCPP server at hand, leave the charge box ID blank and enter the following backend address: `wss://echo.websocket.events/` (this server is sponsored by Lob.com)

**RFID authentication**: Go to "Control Center" > "Connectors" > "Transaction" and update the idTag with the desired value.

## Developers guide

PlatformIO package: [ArduinoOcpp](https://platformio.org/lib/show/11975/ArduinoOcpp)

Please take `examples/ESP/main.cpp` as the starting point for the first project. It is a minimal example which shows how to establish an OCPP connection and how to start and stop charging sessions. The API documentation can be found in [`MicroOcpp.h`](https://github.com/matth-x/MicroOcpp/blob/master/src/MicroOcpp.h). Also check out the [Docs](https://matth-x.github.io/MicroOcpp).

### Dependencies

Mandatory:

- [bblanchon/ArduinoJSON](https://github.com/bblanchon/ArduinoJson)

If compiled with the Arduino integration:

- [Links2004/arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets) (version `2.4.1`)

In case you use PlatformIO, you can copy all dependencies from `platformio.ini` into your own configuration file. Alternatively, you can install the full library with dependencies by adding `matth-x/ArduinoOcpp@0.3.0` in the PIO library manager.

## OCPP 2.0.1 and ISO 15118

**The call for funding for the OCPP 2.0.1 upgrade has been opened.** The version 1.6 support has successfully been funded by private companies who share an interest in using this technology. OCPP is most often seen as a non-differentiating feature of chargers and is therefore perfectly suited for Open Source collaboration. If your company has an interest in the OCPP 2.0.1 upgrade, please use the contact details at the end of this page to receive more details about all advantages of participating in the funding campaign.

ISO 15118 will improve the security and user-friendliness of EV charging. MicroOcpp facilitates the integration of ISO 15118 by handling its OCPP-side communication. This is already being validated at the moment. A public demonstration will follow with the first collaboration on an open OCPP 2.0.1 + ISO 15118 integration.

## Supported Feature Profiles

| Feature profile | supported | in progress |
| -------------- | :---------: | :-----------: |
| **Core** | :heavy_check_mark: |
| **Smart charging** | :heavy_check_mark: |
| **Remote trigger** | :heavy_check_mark: |
| **Firmware management** | :heavy_check_mark: |
| **Local auth list management** | :heavy_check_mark: |
| **Reservation** | :heavy_check_mark: |

Fully OCPP 1.6 compliant :heavy_check_mark:

## Further help

I hope the given documentation and guidance can help you to successfully integrate an OCPP controller into your EVSE. If something needs clarification or if you have a question, please send me a message.

:envelope: : matthias [A⊤] arduino-ocpp [DО⊤] com

If you want professional assistance for your EVSE project, you can contact me as well. I'm looking forward to hearing about your ideas!
