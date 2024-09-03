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

For the flash benchmark, the profiler compiles a [dummy OCPP firmware](https://github.com/matth-x/MicroOcpp/tree/main/tests/benchmarks/firmware_size/main.cpp), analyzes the size of the compilation units using [bloaty](https://github.com/google/bloaty) and evaluates the bloaty report using a [Python script](https://github.com/matth-x/MicroOcpp/tree/main/tests/benchmarks/scripts/eval_firmware_size.py). To give realistic results, the firwmare is compiled with `-Os`, no RTTI or exectiions and newlib as the standard C library. The following tables show the results.

### OCPP 1.6

** Firmware size per Module **

{{ read_csv('modules_v16.csv') }}

### OCPP 2.0.1

** Firmware size per Module **

{{ read_csv('modules_v201.csv') }}

## Full data sets

This section contains the raw data which is the basis for the evaluations above.

** All compilation units for OCPP 1.6 firmware **

{{ read_csv('compile_units_v16.csv') }}

** All compilation units for OCPP 2.0.1 firmware **

{{ read_csv('compile_units_v201.csv') }}
