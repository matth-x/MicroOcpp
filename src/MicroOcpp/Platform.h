// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_PLATFORM_H
#define MO_PLATFORM_H

#include <stdint.h>

#define MO_PLATFORM_NONE    0
#define MO_PLATFORM_ARDUINO 1
#define MO_PLATFORM_ESPIDF  2
#define MO_PLATFORM_UNIX    3

#ifndef MO_PLATFORM
#define MO_PLATFORM MO_PLATFORM_ARDUINO
#endif

#ifdef __cplusplus
namespace MicroOcpp {

void (*getDefaultDebugCb())(const char*);
uint32_t (*getDefaultTickCb())();
uint32_t (*getDefaultRngCb())();

} //namespace MicroOcpp
#endif //__cplusplus

#ifndef MO_MAX_JSON_CAPACITY
#if MO_PLATFORM == MO_PLATFORM_UNIX
#define MO_MAX_JSON_CAPACITY 16384
#else
#define MO_MAX_JSON_CAPACITY 4096
#endif
#endif

#ifndef MO_ENABLE_MBEDTLS
#define MO_ENABLE_MBEDTLS 1
#endif

#endif
