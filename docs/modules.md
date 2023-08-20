# Modules

This chapter gives an overview of the class structure of MicroOcpp.

## Context

The *Context* contains all runtime data of MicroOcpp. Every data object which this library creates is stored in the Context instance, except only the Configuration. So it is the basic entry point to the internals of the library. The structure of the context follows the main architecture as described in [this introduction](intro-tech) and consists of the Request queue and message deserializer for the RPC framework and the Model object for the OCPP model and behavior (see below).

When the library is initialized, `getOcppContext()` returns the current Context object.

## Model

The *Model* represents the OCPP device model and behavior. OCPP defines a rough charger model, i.e. the hardware parts of the charger and their basic functionality in relation to the OCPP operations. Furthermore, OCPP specifies a few only software related features like the reservation of the charger. This charger model is implemented as straightforward C++ data structures and corresponding algorithms.

The implementation of the Model is structured into a top-level Model class and the subordinate Service classes. Each Service class represents a functional block of the OCPP specification and implements the corresponding data structures and functionality. The definition of the functional blocks in MicroOcpp is very similar to the feature profiles in OCPP. Only the Core profile is split into multiple functional blocks to keep a smaller module scope.

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

When being offline, outgoing requests must be queued before sending which is implemented in *RequestQueue*. Queueing is especially challenging for longer offline periods when the number of cached messages exceeds the memory limit. To address this, messages are swapped to the flash memory when the queue limit is reached as implemented in the *RequestStore* and *RequestQueueStorageStrategy* class. Incoming messages can be processed directly and don't have an extensive queueing mechanism.

## Operations

Every OCPP operation (e.g. Heartbeat, BootNotification) has a dedicated class for creating outgoing messages, interpreting incoming messages, executing the specified OCPP action and handling responses. Operations work on the data structures of the Model layer.

To send operations to the OCPP server, they must be wrapped into a Request object. The RPC framework and operations are separated modules. While the RPC framework (including the Request class) deals with the messaging mechanism and transfering data to the other OCPP device, operations define the effect on the OCPP model and data structure and execute the desired action. The operation classes inherit from *Operation* which is the interface visible to the Request class.

Incoming messages are unmarshalled using the *OperationRegistry*. During the initialization phase of the library, the Model classes register all supported operations with their name and an instantiator. The instantiator, when executed, provides the Request interpreter with an instance of the corresponding Operation subclasses. It is possible to extend MicroOcpp by adding new Operation instantiators to the registry, or to modify the behavior by overriding the default Operation implementations. In addition to that, event handlers can be set which the RPC queue will notify with the payload once operations are sent or received.

## Configuration

Configurations like the HeartbeatInterval are managed by the *Configuration* module which consists of

- *AbstractConfiguration*: a single configuration as a key-value pair without value type
    - *Configuration*: a concrete configuration with a value type like `bool` or `const char*`. Inherits from AbstractConfiguration
- *ConfigurationContainer*: a collection of AbstractConfigurations and an optional storage implementation. Multiple containers can be set for a separation of the configurations and different storage strategies. Each container has a unique file name
    - *ConfigurationContainerVolatile*: no persistency and access to the file system
    - *ConfigurationContainerFlash*: persistency by storing JSON files on the flash

If another storage implementation is required (e.g. for syncing with an external configuration manager), then it's possible to add a custom ConfigurationContainer.

In the initialization phase, MicroOcpp loads the built-in Configurations with hard-coded factory defaults and a default storage structure. To customize the factory defaults or which ConfigurationContainers will be used, the Configuration module must be initialized before loading the library. To do so, call `configuration_init(...)`. Then the factory defaults can be applied by calling `declareConfiguration<T>(...)` with the desired default value. To use a custom ConfigurationContainer, call `addConfigurationContainer(...)` with the custom implementation. When the library is loaded afterwards, it will use the previously provided Configurations / Containers and create only the data structure which hasn't been set already.
