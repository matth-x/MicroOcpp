# Getting Started with ArduinoOcpp

This chapter shows how to kick-start your project which deals with OCPP.

## Installation prerequisites

Throughout these document pages, it is assumed that you already have set up your development environment and that you are familiar with the corresponding building, flashing and (basic) debugging routines. ArduinoOcpp runs in many environments (from the Arduino-IDE to proprietary microcontroller IDEs like the Code Composer Studio). If you do not have any preferences yet, it is highly recommended to get started with VSCode + the PlatformIO add-on, since it is the favorite setup of the community and therefore you find the most related information in the Issues pages of the main repository.

There are many high-quality tutorials for out there for setting up VSCode + PIO. The following site covers everything you need to know:
[https://randomnerdtutorials.com/vs-code-platformio-ide-esp32-esp8266-arduino/](https://randomnerdtutorials.com/vs-code-platformio-ide-esp32-esp8266-arduino/)

Once that's done, adding ArduinoOcpp is no big deal anymore. However, let's discuss another very important tool for your project first.

## OCPP Server prerequisites

ArduinoOcpp is just a client, but all the magic of OCPP lives in the communication between a client and a server. Although it *is* possible to run ArduinoOcpp without a real server for testing purposes, the best approach for getting started is to get the hands on a real server. So you can always use the client in a practical setup, see immediate results and simplify development a lot.

Perhaps you were already given access to an OCPP server for your project. Then you can use that, it should work fine. If you don't have a server already, it is highly recommended to get
SteVe ([https://github.com/steve-community/steve](https://github.com/steve-community/steve)).
It allows to control every detail of the OCPP operations and shows detail-rich information about the results. And again, it is the favorite test server of the community, so you will find the most related information on the Web. For the installation instructions, please refer to the
[SteVe docs](https://github.com/steve-community/steve#configuration-and-installation).

In case you can't wait to get started, you can make the first connection test with a WebSocket echo server as a fake OCPP service. ArduinoOcpp supports that: it can send all messages to an echo server which reflects all traffic. ArduinoOcpp gets back its own messages and replies to itself with mocked responses. Complicated, but it does work and the console will show a valid OCPP communication. An example echo server is given in the following section. For the further development though, you will definitely need a real OCPP server.

*Documentation WIP. See the [GitHub Readme](https://github.com/matth-x/ArduinoOcpp) or the [API description](https://github.com/matth-x/ArduinoOcpp/blob/master/src/ArduinoOcpp.h) as reference.*
