# Internal datastructures

The API exposes some of the internal datastructures to the main firmware. This chapter gives an overview of their fields and shows how to use them to achieve the desired behavior.

## Context object

The *Context* contains all runtime data of ArduinoOcpp. Every data object which this library creates is stored in the Context instance, except only the Configuration. So it is the basic entry point to the internals of the library. The structure of the context follows the main architecture as described in [this introduction](intro-tech) and consists of the Request queue and message deserializer for the RPC framework and the Model object for the OCPP model and behavior (see below).

When the library is initialized, `getOcppContext()` returns the current Context object.

## Model object

The *Model* represents the OCPP device model and behavior. OCPP defines a rough charger model, i.e. the hardware parts of the charger and their basic functionality in relation to the OCPP operations. Furthermore, OCPP specifies a few only software related features like the reservation of the charger. This charger model is implemented as straightforward C++ data structures and corresponding algorithms.

The implementation of the Model is structured into a top-level Model class and the subordinate Service classes. Each Service class represents a functional block of the OCPP specification and implements the corresponding data structures and functionality. The definition of the functional blocks in ArduinoOcpp is very similar to the feature profiles in OCPP. Only the Core profile is split into multiple functional blocks to keep a smaller module scope.

The following subsections contain a description of each functional block.

### Authorization

The *AuthorizationService* manages the authorization information of user identifiers or *idTags*. If the local authorization features of OCPP are enabled, then this Service stores a whitelist and blacklist of *idTags* or maintains a cache of recently authorized identifiers. A further responsibility is to determine if the given authorization data is still valid according to the rules in the OCPP specification.

A single *idTag* and its authorization information is represented by the *AuthorizationData* class. It has public getter functions to retreive the authorization information. The list containing those entries is implemented in the *AuthorizationList* class. It has a search function for the *AuthorizationData* by *idTag*.

### Boot

The **BootService** implements the behavior around the *BootNotification* message.

*Documentation WIP. See the [GitHub Readme](https://github.com/matth-x/ArduinoOcpp) or the [API description](https://github.com/matth-x/ArduinoOcpp/blob/master/src/ArduinoOcpp.h) as reference.*
