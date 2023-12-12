# Security

MicroOcpp is designed to be compatible with IoT devices which leads to special considerations regarding cyber security. This section describes the challenges and security concepts of MicroOcpp.

## Challenges of using microcontrollers in safety-critical environments

The two challenges are as follows:

1. Lack of process virtualization in RTOS operating systems
2. Less attention for potential vulnerabilities in the used libraries

In a general purpose OS like Linux, the internet communication modules of an application typically run in a different process than the data base or the hardware supervision / control function. In contrast, on a typical RTOS, all modules are compiled into the same binary, sharing the same address space and lifecycle when being executed. This means that once the network stack crashes, all software on the chip is reset and a vulnerability on the network stack could be exploited to read or manipulate the data of the full runtime environment.

Challenge 2) is due to the fact that OCPP uses standard web technology (WebSocket over TLS), but microcontrollers are missing out the most widespread networking software like OpenSSL or the networking libraries of Linux. The available networking libraries for microcontrollers are also audited well (e.g. lwIP, mbedTLS), but in general there is more attention on potential vulnerabilites in the Linux world, because a huge share of commercial IT systems is based on Linux.

On the upside, an advantage of microcontrollers is their single purpose usage and thus, reduced complexity. Many security breaches are caused by misconfigured and often even superflous software components (e.g. due to overlooked open ports) which are not a regular part of a microcontroller firmware.

## Security measures of MicroOcpp

To address the challenges, the following measures were taken:

- Input sanitazion: MicroOcpp only accepts the JSON format for all input. It is validated by ArduinoJson. Every JSON value is checked against the expected format and for conformity with the OCPP specification before using it. The JSON object is discarded immediately after interpretation
- Transaction safety: to address crashes and random reboots of the microcontroller during operation, all activities of the OCPP library are programmed so that they will either be resumed or fully reverted after reboots, preventing inconsistent states. See also [Transaction safety](../intro-tech/#transaction-safety)
- Careful choice of the dependencies: the mandatory dependency, [ArduinoJson](https://github.com/bblanchon/ArduinoJson), has a test coverage of nearly 100% and is fuzzed. The same goes for the recommended WebSocket library, [Mongoose](https://github.com/cesanta/mongoose). Both projects are very relevant in their field with over 6k and 9k stars on GitHub

Two further measures would be beneficial and could be requested via support request:

- Precautious memory allocation: migrating memory management to the stack and where possible would simplify code analysis and reduce the potential of vulnerabilities
- OCPP fuzzer: as a stateful application protocol, there are specific challenges of developing a fuzzer. An open source fuzzing framework for OCPP could reveal vulnerabilities and be of use for other OCPP projects as well. MicroOcpp is a good foundation for trying new fuzzing approaches. The exposure of the main-loop function and the clock allow a fine-grained access to the program flow and facilitating random alterations of the environment conditions. Furthermore, all persistent data is stored in the JSON format and it is possible to develop a grammatic which contains both a device status and incoming OCPP messages. The Configuration interface could be reused for further status variables which don't need to be persistent in practice, but would improve fuzzing performance when being accessible by the fuzzer.
- Memory pool: object orientation is a very helpful programming paradigm for OCPP. The standard contains a lot of polymorphic entities and optional or variable length data fields. MicroOcpp makes use of the heap and allocates new chunks of memory as the device model is populated with data. On the upside this allows to save a lot of memory during normal operation, but it also entails the risk of memory depletion of the whole controller. A fixed memory pool for OCPP would encapsulate the heap usage to a certain address space and set a hard limit for the memory consumption and avoid polluting the shared heap area by heap fragmentation. To realize the memory pool, it would be necessary to make the allocate and deallocate functions configurable by the client code. Then appropriate (de)allocators can be injected limiting the memory use to a restricted address area. As a consequence, a more thorough allocation error handling in the MicroOcpp code is required and test cases which randomly suppress allocations to test if the library always reverts to a consistent state. A less invasive alternative to memory pools is to inject measured (de)allocators which just prevent the allocation of new memory chunks after a certain threshold has been exceeded. This programming technique would also allow to create much more fine-grained benchmarks of the library.

## Measures to be taken by the EVSE vendor

As a general rule, the communication controller which is exposed to the internet shouldn't be used for safety-critical tasks on the charging hardware. That's because the networking stack is a very complex piece of software which very likely still has open bugs which can crash the controller despite all the effort to improve it. Safety-critical tasks on the charging hardware shouldn't rely on a controller which could crash at any time because of incoming network traffic. To mitigate this, either the OCPP library and internet functionality should be placed onto a separate chip, or the most vital safety functionality should get a dedicated controller.

The recommended [Mongoose WebSocket adapter for MicroOcpp](https://github.com/matth-x/MicroOcppMongoose) supports the OCPP Security Profile 2 (TLS with Basic Authentication) and needs to be provided with the necessary TLS certificate.

Most IoT-controllers have built-in mechanisms to ensure the authenticity of their firmware. For example, the Espressif32 supports [Secure Boot](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/secure-boot-v2.html) which is a signature verification of the installed firmware before that firmware is executed. Many platforms also have a built-in signature verification for incoming OTA firmware updates. To prove the authenticity of the charger to the OCPP server, it is also important to keep the WebSocket key secret by [encrypting the flash memory](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/flash-encryption.html). These security mechanisms heavily depend on the host controller which runs MicroOcpp. It is the responsibility of the main firmware to make proper use of them.

## OCPP Security Whitepaper and ISO 15118

With MicroOcpp, the recommended way of handling certificates on microcontrollers is to compile them into the firmware binary and to rely on the built-in firmware signature checks of the host microcontroller platform. This lean approach results in a smaller attack vector compared to establishing a separate infrastructure for the server- and firmware certificate. It can be assumed that the OTA functionality of the microcontrollers is thoroughly tested and consequently, reaching a comparable level of robustness would require much effort.

In case the certificate handling mechanism of the Security Whitepaper is preferred, then the EVSE vendor needs to implement it via a custom extension. Unfortunately, this mechanism hasn't been requested yet and is not natively supported by MicroOcpp yet. The new custom operations can be implemented by extending the class `Operation`. A handler for incoming messages can be registered via `OperationRegistry::registerOperation(...)`. To send custom messages to the server, use `Context::initiateRequest(...)`.

A further challenge for microcontrollers is the relatively low processor speed which becomes relevant for a potential ISO 15118 integration. Some incoming message types (`AuthorizationReq` and `MeteringReceiptReq`) include a signature which needs to be verified on the communications controller of the EVSE. Moreover, messages in the ISO 15118 V2G protocol have a maximum round trip time (which is 2 seconds for the message types in question) and so the signature verification is time-contrained. [These benchmarks](https://web.archive.org/web/20230724184529/https://www.oryx-embedded.com/benchmark/espressif/crypto-esp32.html) for the Espressif32 show that for some signature algorithms, the verification time can get close or exceed the timing requirements of ISO 15118 if done on the processor only. As a consequence, hardware acceleration by the crypto-core is mandatory to ensure a robust communication between the EVSE and EV. Before making a communications controller with ISO 15118 support, the performance of the host controller should be benchmarked and checked against the requirements.

*Disclaimer: the outlined risks in this section are not a complete list. Also, every system has unique security challenges which require individual attention. In doubt, please consult an IT-security specialist.*
