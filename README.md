# ESP8266-OCPP
OCPP 1.6 Smart Charging client for ESP8266

## Get the Smart Grid on your Home Charger :car::electric_plug::battery:

One of the biggest milestones for the Smart Grid is the full integration of EVs into managed Charging systems. An open and homogenous protocol landscape is a key factor to reduce the effort to establish grid-wide Smart Charging strategies. The Open Charge Point Protocol (OCPP) is mainly designed for public EVSEs, but brings an extensive basis for Smart Charging in private chargers as well. This project intends to provide the OCPP framework for turning an ESP8266 into a Smart Charging controller.

By now, an OCPP-enabled ESP8266 was successfully integrated into a ClipperCreek Inc. charging station. That controller connected the charging station to a third-party load management system. In the tests, the ESP8266 proved to be a suitable platform. Furthermore, the test results promise that the ESP8266 is most likely powerful enough for full OCPP.

As the repository name indicates, this project is not limited to Smart Charging. The framework is open to extension by any OCPP message type and is designed towards all OCPP functionalities. But for now, the main development is focused on Smart Charging since it is the most important use case on private chargers. In future, the focus on private chargers could also move towards all types of EVSEs.

## Dependencies

- bblanchon/ArduinoJSON
- Links2004/arduinoWebSockets
- ivanseidel/LinkedList
- PaulStoffregen/Time

## Next development steps

- introduce a timeout mechanism, proper offline behavior and package loss / fault detection
- handle fragmented input messages correctly
- some refactoring steps (encapsulate message object instantiation in a factory, maybe introduce a facade for the framework, etc.)
- decouple the framework from the WebSockets library; This enables OCPP server operation (e.g. as Local Smart Charging controller) with only little extra effort
- add support for multiple power connectors
- reach full compliance to OCPP 1.6 Smart Charging Profile