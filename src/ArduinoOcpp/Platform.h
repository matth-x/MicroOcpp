// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef AO_PLATFORM_H
#define AO_PLATFORM_H

#ifdef __cplusplus
#define EXT_C extern "C"
#else
#define EXT_C
#endif

#define AO_PLATFORM_ARDUINO 0
#define AO_PLATFORM_ESPIDF  1
#define AO_PLATFORM_UNIX    2

#ifndef AO_PLATFORM
#define AO_PLATFORM AO_PLATFORM_ARDUINO
#endif

#ifdef AO_CUSTOM_CONSOLE
#include <cstdio>

#ifndef AO_CUSTOM_CONSOLE_MAXMSGSIZE
#define AO_CUSTOM_CONSOLE_MAXMSGSIZE 196
#endif

void ao_set_console_out(void (*console_out)(const char *msg));

namespace ArduinoOcpp {
void ao_console_out(const char *msg);
}
#define AO_CONSOLE_PRINTF(X, ...) \
            do { \
                char msg [AO_CUSTOM_CONSOLE_MAXMSGSIZE]; \
                if (snprintf(msg, AO_CUSTOM_CONSOLE_MAXMSGSIZE, X, ##__VA_ARGS__) < 0) { \
                    sprintf(msg + AO_CUSTOM_CONSOLE_MAXMSGSIZE - 7, " [...]"); \
                } \
                ArduinoOcpp::ao_console_out(msg); \
            } while (0)
#else
#define ao_set_console_out(X) \
            do { \
                X("[AO] CONSOLE ERROR: ao_set_console_out ignored if AO_CUSTOM_CONSOLE " \
                  "not defined\n"); \
                char msg [196]; \
                snprintf(msg, 196, "     > see %s:%i",__FILE__,__LINE__); \
                X(msg); \
                X("\n     > see ArduinoOcpp/Platform.h\n"); \
            } while (0)

#if AO_PLATFORM == AO_PLATFORM_ARDUINO
#include <Arduino.h>
#ifndef AO_USE_SERIAL
#define AO_USE_SERIAL Serial
#endif

#define AO_CONSOLE_PRINTF(X, ...) AO_USE_SERIAL.printf_P(PSTR(X), ##__VA_ARGS__)
#elif AO_PLATFORM == AO_PLATFORM_ESPIDF || AO_PLATFORM == AO_PLATFORM_UNIX
#include <stdio.h>

#define AO_CONSOLE_PRINTF(X, ...) printf(X, ##__VA_ARGS__)
#endif
#endif

#ifdef AO_CUSTOM_TIMER
void ao_set_timer(unsigned long (*get_ms)());

unsigned long ao_tick_ms_custom();
#define ao_tick_ms ao_tick_ms_custom
#else

#if AO_PLATFORM == AO_PLATFORM_ARDUINO
#include <Arduino.h>
#define ao_tick_ms millis
#elif AO_PLATFORM == AO_PLATFORM_ESPIDF
unsigned long ao_tick_ms_espidf();
#define ao_tick_ms ao_tick_ms_espidf
#elif AO_PLATFORM == AO_PLATFORM_UNIX
unsigned long ao_tick_ms_unix();
#define ao_tick_ms ao_tick_ms_unix
#endif
#endif

#ifndef AO_MAX_JSON_CAPACITY
#if AO_PLATFORM == AO_PLATFORM_UNIX
#define AO_MAX_JSON_CAPACITY 16384
#else
#define AO_MAX_JSON_CAPACITY 4096
#endif
#endif

#if AO_PLATFORM != AO_PLATFORM_ARDUINO
void dtostrf(float value, int min_width, int num_digits_after_decimal, char *target);
#endif

#endif
