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

## Main-loop paradigm

ArduinoOcpp works with the common main-loop execution model of microcontrollers. After initialization, the EVSE firmware most likely enters a main-loop and repeats it infinitely. To run ArduinoOcpp, a call to its loop function must be placed into the main loop of the firmware. Then at each main-loop iteration, ArduinoOcpp executes its internal routines, i.e. it processes input data, updates its data model, executes operations and creates new output data. The ArduinoOcpp loop function does not block the main loop but executes immediately. This library does not contain any delay functions. Some activities of the library spread over many loop iterations like the start of a charging session which needs to await the approval of an NFC card and a hardware diagnosis of the high power electronics for example. All activities in ArduinoOcpp support the distribution over many loop calls, leading to a pseudo-parallel execution behavior.

No separate RTOS task is needed and ArduinoOcpp does not have an internal mechanism for multi-task synchronization. However, it is of course possible to create a dedicated OCPP task, as long as extra care is taken of the synchronization.

## How the API works

The high-level API consists of four parts:

- **Library lifecycle**: The library has initialize functions with a few initialization options. Dynamic system components like the WebSocket adapter need to be set at initialization time. The deinitialize function reverts the library into an unitialized state. That's useful for memory inspection tools like valgrind or to disable the OCPP communication. The loop function also counts as part of the lifecycle management.
- **Sensor Inputs**: EVSEs are mechanical systems with a variety of sensor information. OCPP is used to send parts of the sensor readings to the server. The other part of the sensor data flows into the local charger model of ArduinoOcpp where it is further processed. To update ArduinoOcpp with the input data from the sensors, the firmware needs to bind the sensors to the library. An *Input-binding*, or in short *Input*, is a function which transfers the current sensor value to ArduinoOcpp. Inputs are callback functions which read a specific sensor value and pass the value in the return statement. The firmware defines those callback functions for each sensor and adds them to ArduinoOcpp during initialization. After initialization, ArduinoOcpp uses the callbacks and executes them to fetch the most recent sensor values. <br/>
This concept is reused for the data *Outputs* of the library to the firmware, where the callback applies output data from ArduinoOcpp to the firmware.
- **Transaction management**: OCPP considers EVSEs as vending machines. To enable payment processing and the billing of the EVSE usage, all charging activity is assigned to transactions. A big portion of OCPP is about transactions, their prerequisites, runtime and their termination scenarios. The ArduinoOcpp API breaks transactions down into an initiation and termination function and gives a transparent view on the current process status, authorization result and offline behavior strategy. For non-commercial setups, the transaction mechanism is the same but has only informational purposes.
- **Device management**: ArduinoOcpp implements the OCPP side of the device management operations. For the actual execution, the firmware needs to provide the charger-side implementations of the operations to ArduinoOcpp by passing handler functions to the API. For example, the OCPP server can restart the charger. Upon receipt of the request, ArduinoOcpp terminates the transactions and eventually triggers the system restart using the handler function which the firmware has provided through the high-level API.

## Transaction safety

Software in EVSEs needs to withstand hazardous operating conditions. EVSEs are located on the street or in garages where the WiFi or LTE signal strength is often weak, leading to long offline periods or where random power cuts can occur. In addition to that, the lack of process virtualization on microcontrollers means that a malfunction in one part of the firmware leads to the crash of all other parts.

The transaction process of ArduinoOcpp is robust against random failures or resets. A minimal transaction log on the flash storage ensures that each operation on a transaction is fully executed. It will always result in a consistent state between the EVSE and the OCPP server, even over resets of the microcontroller. The RPC queue facilitates this by tracking the delivery status of relevant messages. If the microcontroller is reset while the delivery status of a message is unknown, ArduinoOcpp takes up the message delivery again at the next start up and completes it.

A requirement for the transaction safety feature is the availability of a journaling file system. Examples include LittleFS, SPIFFS and the POSIX file API, but some microcontroller platforms don't support this natively, so an extension would be required.

## Unit testing

ArduinoOcpp includes a number of unit tests based on the [Catch2](https://github.com/catchorg/Catch2) framework. A [GitHub Action](https://github.com/matth-x/ArduinoOcpp/actions) runs the unit tests against each new commit in the ArduinoOcpp repository, which ensures that new features don't break old code.

The scope of the unit tests is to to ensure a correct implementation of OCPP and to validate the high-level API against its definition. For that, it is not necessary to establish an actual test connection to an OCPP server. In fact, real-world communication would disturb the tests and make them undeterministic. That's why the test suite is fully based on an integrated, tiny OCPP test server which the OCPP client reaches over a loopback connection. The test suite does not access the WebSocket library. When making the unit tests of the main firmware, it is not necessary to check the full OCPP communication, but only to validate correct usage of the high-level API. An example of how the library can be initialized with a loopback connection can be found in its test suite.

*Documentation WIP. See the [GitHub Readme](https://github.com/matth-x/ArduinoOcpp) or the [API description](https://github.com/matth-x/ArduinoOcpp/blob/master/src/ArduinoOcpp.h) as reference.*
