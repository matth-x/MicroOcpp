# Modules

This chapter gives an overview of the class structure of ArduinoOcpp.

## Context

The *Context* contains all runtime data of ArduinoOcpp. Every data object which this library creates is stored in the Context instance, except only the Configuration. So it is the basic entry point to the internals of the library. The structure of the context follows the main architecture as described in [this introduction](intro-tech) and consists of the Request queue and message deserializer for the RPC framework and the Model object for the OCPP model and behavior (see below).

When the library is initialized, `getOcppContext()` returns the current Context object.

## Model

The *Model* represents the OCPP device model and behavior. OCPP defines a rough charger model, i.e. the hardware parts of the charger and their basic functionality in relation to the OCPP operations. Furthermore, OCPP specifies a few only software related features like the reservation of the charger. This charger model is implemented as straightforward C++ data structures and corresponding algorithms.

The implementation of the Model is structured into a top-level Model class and the subordinate Service classes. Each Service class represents a functional block of the OCPP specification and implements the corresponding data structures and functionality. The definition of the functional blocks in ArduinoOcpp is very similar to the feature profiles in OCPP. Only the Core profile is split into multiple functional blocks to keep a smaller module scope.

The following list contains the resulting functional blocks:

- **Authorization**: local information of user identifiers and their authorization status
- **Boot**: implementation of the *preboot* behavior, i.e. sending and processing the BootNotification message
- **ChargingSession**: management of charging sessions and control of the high power charging hardware
- **Diagnostics**: GetDiagnostics upload routine
- **FirmwareManagement**: UpdateFirmware download routine
- **Heartbeat**: periodic OCPP Heartbeats (not including WebSocket ping-pongs)
- **Metering**: periodic MeterValue messages and local caching
- **Reservation**: management of Reservation lists and their effect on the authorization routine
- **Reset**: execution of OCPP Reset message
- **Transactions**: transaction journal behind StartTransaction and StopTransaction messages and *Transaction* class for extensions of the transaction mechanism

## Requests

The *Request* class and all similarly named classes implement the Remote Procdure Call (RPC) framework of OCPP. A request executes an operation on the remote end of an OCPP connection. If a charger sends a request to a server, then the server will update its data base with the payload and vice versa. After receiving a request, each node replies with a confirmation, acknowledging the successful execution of the operation or notifying about an error.

When being offline, outgoing requests must be queued before sending which is implemented in *RequestQueue*. Queueing is especially challenging for longer offline periods when the number of cached messages exceeds memory limits. To address this, messages are swapped to the flash memory when the queue limit is reached as implemented in the *RequestStore* and *RequestQueueStorageStrategy* class. Incoming messages can be processed directly and don't have an extensive queueing mechanism.

*Documentation WIP. See the [GitHub Readme](https://github.com/matth-x/ArduinoOcpp) or the [API description](https://github.com/matth-x/ArduinoOcpp/blob/master/src/ArduinoOcpp.h) as reference.*
