# <img src="https://user-images.githubusercontent.com/63792403/133922028-fefc8abb-fde9-460b-826f-09a458502d17.png" alt="Icon" height="24"> &nbsp; MicroOcpp (ArduinoOcpp)

[![Build Status]( https://github.com/matth-x/MicroOcpp/workflows/PlatformIO%20CI/badge.svg)](https://github.com/matth-x/MicroOcpp/actions)
[![Unit tests]( https://github.com/matth-x/MicroOcpp/workflows/Unit%20tests/badge.svg)](https://github.com/matth-x/MicroOcpp/actions)
[![codecov](https://codecov.io/github/matth-x/ArduinoOcpp/branch/develop/graph/badge.svg?token=UN6LO96HM7)](https://codecov.io/github/matth-x/ArduinoOcpp)

OCPP 1.6 client for microcontrollers. Portable C/C++. Compatible with Espressif, Arduino, NXP, STM and Linux and more.

**Formerly ArduinoOcpp ([migration guide](https://matth-x.github.io/MicroOcpp/migration/))**: *since the first release in 2020, ArduinoOcpp has increasingly been used on other platforms than Arduino. The goal of this project has evolved to fit seamlessly in a high variety of microcontroller firmwares and so the dependency on Arduino was dropped some time ago. The new name reflects the purpose of this project better and prevents potential confusion. Despite the new name, nothing changes for existing users and the Arduino integration will continue to be fully functional.*

Reference usage: [OpenEVSE](https://github.com/OpenEVSE/ESP32_WiFi_V4.x/blob/master/src/ocpp.cpp) | Technical introduction: [Docs](https://matth-x.github.io/MicroOcpp/intro-tech) | Website: [www.arduino-ocpp.com](https://www.arduino-ocpp.com)

:heavy_check_mark: Works with [SteVe](https://github.com/RWTH-i5-IDSG/steve) and [15+ commercial Central Systems](https://www.arduino-ocpp.com/#h.314525e8447cc93c_81)

:heavy_check_mark: Eligible for public chargers (Eichrecht-compliant)

:heavy_check_mark: Supports all OCPP 1.6 feature profiles

## Tester / Demo App

*Main repository: [MicroOcppSimulator](https://github.com/matth-x/MicroOcppSimulator)*

(Beta) The Simulator is a demo & development tool for MicroOcpp which allows to quickly assess the compatibility with different OCPP backends. It simulates a full charging station, adds a GUI and a mocked hardware binding to MicroOcpp and runs in the browser (using WebAssembly): [Try it](https://demo.micro-ocpp.com/)

[![Screenshot](https://github.com/agruenb/arduino-ocpp-dashboard/blob/master/docs/img/status_page.png)](https://demo.micro-ocpp.com/)

#### Usage

**OCPP server setup**: Navigate to "Control Center". In the WebSocket options, add the OCPP backend URL, charge box ID and authorization key if existent. Press "Update WebSocket" to save. The Simulator should connect to the OCPP server. To check the connection status, it could be helpful to open the developer tools of the browser.

If you don't have an OCPP server at hand, leave the charge box ID blank and enter the following backend address: `wss://echo.websocket.events/` (this server is sponsored by Lob.com)

**RFID authentication**: Go to "Control Center" > "Connectors" > "Transaction" and update the idTag with the desired value.

## Developers guide

PlatformIO package: [ArduinoOcpp](https://platformio.org/lib/show/11975/ArduinoOcpp)

MicroOcpp is an implementation of the OCPP communication behavior. It automatically initiates the corresponding OCPP operations once the hardware status changes or the RFID input is updated with a new value. Conversely it processes new data from the server, stores it locally and updates the hardware controls when applicable.

Please take `examples/ESP/main.cpp` as the starting point for the first project. It is a minimal example which shows how to establish an OCPP connection and how to start and stop charging sessions. The API documentation can be found in [`MicroOcpp.h`](https://github.com/matth-x/MicroOcpp/blob/master/src/MicroOcpp.h). Also check out the [Docs](https://matth-x.github.io/MicroOcpp).

### Dependencies

Mandatory:

- [bblanchon/ArduinoJSON](https://github.com/bblanchon/ArduinoJson)

If compiled with the Arduino integration:

- [Links2004/arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets) (version `2.4.1`)

In case you use PlatformIO, you can copy all dependencies from `platformio.ini` into your own configuration file. Alternatively, you can install the full library with dependencies by adding `matth-x/ArduinoOcpp@0.3.0` in the PIO library manager.

## OCPP 2.0.1 and ISO 15118

**The call for funding for the OCPP 2.0.1 upgrade is open.** The version 1.6 support has successfully been funded by private companies who share an interest in using this technology. OCPP is most often seen as a non-differentiating feature of chargers and is therefore perfectly suited for Open Source collaboration. If your company has an interest in the OCPP 2.0.1 upgrade, please use the contact details at the end of this page to receive more details about all advantages of participating in the funding campaign.

MicroOcpp facilitates the integration of ISO 15118 by handling its OCPP-side communication. This is already being validated at the moment. A public demonstration will follow with the first collaboration on an open OCPP 2.0.1 + ISO 15118 integration.

## Contact details

I hope the given documentation and guidance can help you to successfully integrate an OCPP controller into your EVSE. I will be happy if you reach out!

:envelope: : matthias [A⊤] arduino-ocpp [DО⊤] com
