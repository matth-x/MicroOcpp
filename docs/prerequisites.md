# Development tools and basic prerequisites

This page explains how to work with this library using the appropriate development tools. Skip it if your IDE is set up and you already have an OCPP test server.

## Development tools prerequisites

Throughout these document pages, it is assumed that you already have set up your development environment and that you are familiar with the corresponding building, flashing and (basic) debugging routines. MicroOcpp runs in many environments (from the Arduino-IDE to proprietary microcontroller IDEs like the Code Composer Studio). If you do not have any preferences yet, it is highly recommended to get started with VSCode + the PlatformIO add-on, since it is the favorite setup of the community and therefore you find the most related information in the Issues pages of the main repository.

There are many high-quality tutorials for out there for setting up VSCode + PIO. The following site covers everything you need to know:
[https://randomnerdtutorials.com/vs-code-platformio-ide-esp32-esp8266-arduino/](https://randomnerdtutorials.com/vs-code-platformio-ide-esp32-esp8266-arduino/)

Once that's done, adding MicroOcpp is no big deal anymore. However, let's discuss another very important tool for your project first.

## OCPP Server prerequisites

MicroOcpp is just a client, but all the magic of OCPP lives in the communication between a client and a server. Although it *is* possible to run MicroOcpp without a real server for testing purposes, the best approach for getting started is to get the hands on a real server. So you can always use the client in a practical setup, see immediate results and simplify development a lot.

Perhaps you were already given access to an OCPP server for your project. Then you can use that, it should work fine. If you don't have a server already, it is highly recommended to get
SteVe ([https://github.com/steve-community/steve](https://github.com/steve-community/steve)).
It allows to control every detail of the OCPP operations and shows detail-rich information about the results. And again, it is the favorite test server of the community, so you will find the most related information on the Web. For the installation instructions, please refer to the
[SteVe docs](https://github.com/steve-community/steve#configuration-and-installation).

In case you can't wait to get started, you can make the first connection test with a WebSocket echo server as a fake OCPP service. MicroOcpp supports that: it can send all messages to an echo server which reflects all traffic. MicroOcpp gets back its own messages and replies to itself with mocked responses. Complicated, but it does work and the console will show a valid OCPP communication. An example echo server is given in the following section. For the further development though, you will definitely need a real OCPP server.

## Project structure

MicroOcpp is a library, i.e. it is not a full firmware, but just solves one specific task in your project which is the OCPP connectivity. The project structure should reflect this: typically you download MicroOcpp into a libraries or dependencies subfolder, while the main part of the development takes place in a main source folder. All dependencies of MicroOcpp (i.e. ArduinoJson, see the dependencies sections) should be located in the same libraries or dependencies folder.

When the include paths are correctly set up, you should be able `#include <MicroOcpp.h>` at the top of your own source files. This setup keeps the OCPP library source separate from your integration and gives the project a clear structure.

## Dependency managers

Currently, the PlatformIO dependency manager is supported. In the `platformio.ini` manifest, you can add `matth-x/MicroOcpp` to the `lib_deps` section.
