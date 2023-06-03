# Technical introduction

This chapter covers everything you need to know to understand ArduinoOcpp.

## Scope of ArduinoOcpp

The OCPP specification defines a charger data model, operations on the data model and the resulting physical behavior on the charger side. ArduinoOcpp implements the full scope of OCPP, i.e. a minimalistic data store for the data model, the OCPP operations and an interface to the surrounding firmware.

Another part of OCPP is its messaging mechanism, the so-called Remote Procedure Calls (RPC) framework. ArduinoOcpp also implements the specified RPC framework with the required guarantees of message delivery or the corresponding error handling.

At the lowest layer, OCPP relies on standard WebSockets. ArduinoOcpp works with any WebSocket library and has a lean interface to integrate them.

The high-level API in `ArduinoOcpp.h` bundles all touch points of the EVSE firmware with the OCPP library.

<p style="text-align:center">
    <img src="../img/components_overview.svg">
    <br />
    <em>Overview of the architecture</em>
</p>

## High-level OCPP support

Being a full implementation of OCPP, ArduinoOcpp handles the OCPP communication, i.e. it sends OCPP requests and processes incoming OCPP requests autonomously. The messages are triggered by the internal data model and by input from the high-level API. Incoming OCPP requests are used to update the internal data model and if an action on the charger is required, the library signals that to the main firmware through the high-level API.

In consequence, the high-level API decouples the main firmware from the OCPP communication and hides the operations. This has the following good reasons:

- The high-level API guarantees correctnes of the OCPP integration. As soon as the charger adopts it properly, it is fully OCPP-compliant
- The hardware-near design decreases the integration effort into the firmware hugely
- The API won't change substantially for the OCPP 2.0.1 upgrade. The EVSE will get OCPP 2.0.1 support on the fly by a later firmware update

## Customizability

One core principle of the architecture of ArduinoOcpp is the customizability and the selective usage of its components.

Selective usage of components means that the EVSE firmware can use parts of ArduinoOcpp and work with its own implementation for the rest. In that case only the selected parts of ArduinoOcpp will be compiled into the firmware. For example, the main firmware can use the RPC framework and build a custom implementation of the OCPP logic on top of it. This could be necessary if the OCPP behavior should be tightly coupled to other modules of the firmware. In a different scenario, the EVSE firmware could already contain an extensive RPC framework and the OCPP client should reuse it. Then, only the business logic and high-level API are of interest.

<p style="text-align:center">
    <img src="../img/components_selective.svg">
    <br />
    <em>Selective usage of ArduinoOcpp</em>
</p>

Customizations of the library allow to integrate use cases for which the high-level API is too restrictive. The high-level API is designed to provide a facade for the expected usage of the library, but since the charging sector is driven by innovation, new use cases for OCPP emerge every day. If a custom use case cannot be integrated on the API level, the main firmware can access the internal data structures of ArduinoOcpp and complement the required functionality or replace parts of the internal behavior with custom implementations which fits the concrete scenarios better.

*Documentation WIP. See the [GitHub Readme](https://github.com/matth-x/ArduinoOcpp) or the [API description](https://github.com/matth-x/ArduinoOcpp/blob/master/src/ArduinoOcpp.h) as reference.*
