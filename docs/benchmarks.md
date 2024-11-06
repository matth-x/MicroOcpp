# Benchmarks

Microcontrollers have tight hardware constraints which affect how much resources the firmware can demand. It is important to make sure that the available resources are not depleted to allow for robust operation and that there is sufficient flash head room to allow for future software upgrades.

In general, microcontrollers have three relevant hardware constraints:

- Limited processing speed
- Limited memory size
- Limited flash size

For OCPP, the relevant bottlenecks are especially the memory and flash size. The processing speed is no concern, since OCPP is not computationally complex and does not include any extensive planning algorithms on the charger size. A previous [benchmark on the ESP-IDF](https://github.com/matth-x/MicroOcpp-benchmark) showed that the processing times are in the lower milliseconds range and are probably outweighed by IO times and network round trip times.

However, the memory and flash requirements are important figures, because the device model of OCPP has a significant size. The microcontroller needs to keep the model data in the heap memory for the largest part and the firmware which covers the corresponding processing routines needs to have sufficient space on flash.

This chapter presents benchmarks of the memory and flash requirements. They should help to determine the required microcontroller capabilities, or to give general insights for taking further action on optimizing the firmware.

## Firmware size

When compiling a firmware with MicroOCPP, the resulting binary will contain functionality which is not related to OCPP, like hardware drivers, modules which are shared, like MbedTLS and the actual MicroOCPP object files. The size of the latter is the final flash requirement of MicroOCPP.

For the flash benchmark, the profiler compiles a [dummy OCPP firmware](https://github.com/matth-x/MicroOcpp/tree/main/tests/benchmarks/firmware_size/main.cpp), analyzes the size of the compilation units using [bloaty](https://github.com/google/bloaty) and evaluates the bloaty report using a [Python script](https://github.com/matth-x/MicroOcpp/tree/main/tests/benchmarks/scripts/eval_firmware_size.py). To give realistic results, the firwmare is compiled with `-Os`, no RTTI or exceptions and newlib as the standard C library. The following tables show the results.

### OCPP 1.6

The following table shows the cumulated size of the objects files per module. The Module category consists of the OCPP 2.0.1 functional blocks, OCPP 1.6 feature profiles and general functionality which is shared accross the library. If a feature of the implementation falls under both an OCPP 2.0.1 functional block and OCPP 1.6 feature profile definition, it is preferrably assigned to the OCPP 2.0.1 category. This allows for better comparability between both OCPP versions.

**Table 1: Firmware size per Module**

{{ read_csv('modules_v16.csv') }}

### OCPP 2.0.1

**Table 2: Firmware size per Module**

{{ read_csv('modules_v201.csv') }}

## Memory usage

MicroOCPP uses the heap memory to process incoming messages, maintain the device model and create outgoing OCPP messages. The total heap usage should remain low enough to not risk a heap depletion which would not only affect the OCPP module, but the whole controller, because heap memory is typically shared on microcontrollers. To assess the heap usage of MicroOCPP, a test suite runs a variety of simulated charger use cases and measures the maximum occupied memory. Then, the maximum observed value is considered as the memory requirement of MicroOCPP.

Another important figure is the base level which is much closer to the average heap usage. The total heap usage consists of a base level and a dynamic part. Some memory objects are only initialized once during startup or as the device model is populated (e.g. Charging Schedules) and therefore belong to the base which changes only slowly over time. In contrast, objects for the JSON parsing and serialization and the internal execution of the operations are highly dynamic as they are instantiated for one operation and freed again after completion of the action. If the firmware contains multiple components besides MicroOCPP with this usage pattern, then the average total memory occupation of the device RAM is even closer to the base levels of the individual components.

The following table shows the dynamic heap usage for a variety of test cases, followed by the base level and resulting maximum memory occupation of MicroOCPP. At the time being, the measurements are limited to only OCPP 2.0.1 and a narrow set of test cases. They will be gradually extended over time.

**Table 3: Memory usage per use case and total**

{{ read_csv('heap_v201.csv') }}

## Full data sets

This section contains the raw data which is the basis for the evaluations above.

**Table 4: All compilation units for OCPP 1.6 firmware**

{{ read_csv('compile_units_v16.csv') }}

**Table 5: All compilation units for OCPP 2.0.1 firmware**

{{ read_csv('compile_units_v201.csv') }}
