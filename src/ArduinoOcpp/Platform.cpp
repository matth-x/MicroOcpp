// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Platform.h>

#ifdef AO_CUSTOM_CONSOLE

namespace ArduinoOcpp {
void (*ao_console_out_impl)(const char *msg) = nullptr;
}

void ArduinoOcpp::ao_console_out(const char *msg) {
    if (ao_console_out_impl) {
        ao_console_out_impl(msg);
    }
}

void ao_set_console_out(void (*console_out)(const char *msg)) {
    ArduinoOcpp::ao_console_out_impl = console_out;
    console_out("[AO] console initialized\n");
}

#endif

#ifdef AO_CUSTOM_TIMER
unsigned long (*ao_tick_ms_impl)() = nullptr;

void ao_set_timer(unsigned long (*get_ms)()) {
    ao_tick_ms_impl = get_ms;
}

unsigned long ao_tick_ms_custom() {
    if (ao_tick_ms_impl) {
        return ao_tick_ms_impl();
    } else {
        return 0;
    }
}
#else

#if AO_PLATFORM == AO_PLATFORM_ESPIDF
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace ArduinoOcpp {

decltype(xTaskGetTickCount()) ocpp_ticks_count = 0;
unsigned long ocpp_millis_count = 0;

}

unsigned long ao_tick_ms_espidf() {
    auto ticks_now = xTaskGetTickCount();
    ArduinoOcpp::ocpp_millis_count += ((ticks_now - ArduinoOcpp::ocpp_ticks_count) * 1000UL) / configTICK_RATE_HZ;
    ArduinoOcpp::ocpp_ticks_count = ticks_now;
    return ArduinoOcpp::ocpp_millis_count;
}

#elif AO_PLATFORM == AO_PLATFORM_UNIX
#include <chrono>

namespace ArduinoOcpp {

std::chrono::steady_clock::time_point clock_reference;
bool clock_initialized = false;

}

unsigned long ao_tick_ms_unix() {
    if (!ArduinoOcpp::clock_initialized) {
        ArduinoOcpp::clock_reference = std::chrono::steady_clock::now();
        ArduinoOcpp::clock_initialized = true;
    }
    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - ArduinoOcpp::clock_reference);
    return (unsigned long) ms.count();
}
#endif
#endif

#if AO_PLATFORM != AO_PLATFORM_ARDUINO
void dtostrf(float value, int min_width, int num_digits_after_decimal, char *target){
    char fmt[20];
    sprintf(fmt, "%%%d.%df", min_width, num_digits_after_decimal);
    sprintf(target, fmt, value);
}
#endif
