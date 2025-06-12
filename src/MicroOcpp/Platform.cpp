// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#include <MicroOcpp/Platform.h>

#if MO_PLATFORM == MO_PLATFORM_ARDUINO
#include <Arduino.h>
#ifndef MO_USE_SERIAL
#define MO_USE_SERIAL Serial
#endif

namespace MicroOcpp {

void defaultDebugCbImpl(const char *msg) {
    MO_USE_SERIAL.printf("%s", msg);
}

unsigned long defaultTickCbImpl() {
    return millis();
}

} //namespace MicroOcpp

#elif MO_PLATFORM == MO_PLATFORM_ESPIDF
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace MicroOcpp {
    
void defaultDebugCbImpl(const char *msg) {
    printf("%s", msg);
}

decltype(xTaskGetTickCount()) mocpp_ticks_count = 0;
unsigned long mocpp_millis_count = 0;

unsigned long defaultTickCbImpl() {
    auto ticks_now = xTaskGetTickCount();
    mocpp_millis_count += ((ticks_now - mocpp_ticks_count) * 1000UL) / configTICK_RATE_HZ;
    mocpp_ticks_count = ticks_now;
    return mocpp_millis_count;
}
} //namespace MicroOcpp

#elif MO_PLATFORM == MO_PLATFORM_UNIX
#include <stdio.h>
#include <chrono>

namespace MicroOcpp {
    
void defaultDebugCbImpl(const char *msg) {
    printf("%s", msg);
}

std::chrono::steady_clock::time_point clock_reference;
bool clock_initialized = false;

unsigned long defaultTickCbImpl() {
    if (!clock_initialized) {
        clock_reference = std::chrono::steady_clock::now();
        clock_initialized = true;
    }
    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - clock_reference);
    return (unsigned long) ms.count();
}

} //namespace MicroOcpp
#else
namespace MicroOcpp {
void (*defaultDebugCbImpl)(const char*) = nullptr
unsigned long (*defaultTickCbImpl)() = nullptr;
} //namespace MicroOcpp
#endif

namespace MicroOcpp {

void (*getDefaultDebugCb())(const char*) {
    return defaultDebugCbImpl;
}

unsigned long (*getDefaultTickCb())() {
    return defaultTickCbImpl;
}

// Time-based Pseudo RNG.
// Contains internal state which is mixed with the current timestamp
// each time it is called. Then this is passed through a multiply-with-carry
// PRNG operation to get a pseudo-random number.
uint32_t defaultRngCbImpl(void) {
    static uint32_t prng_state = 1;
    uint32_t entropy = defaultTickCbImpl(); //ensure that RNG is only executed when defaultTickCbImpl is defined
    prng_state = (prng_state ^ entropy)*1664525U + 1013904223U; // assuming complement-2 integers and non-signaling overflow
    return prng_state;
}

uint32_t (*getDefaultRngCb())() {
    if (!defaultTickCbImpl) {
        //default RNG depends on default TickCb
        return nullptr;
    }
    return defaultRngCbImpl;
}

} //namespace MicroOcpp
