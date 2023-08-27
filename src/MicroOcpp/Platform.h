// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef MOCPP_PLATFORM_H
#define MOCPP_PLATFORM_H

#ifdef __cplusplus
#define EXT_C extern "C"
#else
#define EXT_C
#endif

#define MOCPP_PLATFORM_NONE    0
#define MOCPP_PLATFORM_ARDUINO 1
#define MOCPP_PLATFORM_ESPIDF  2
#define MOCPP_PLATFORM_UNIX    3

#ifndef MOCPP_PLATFORM
#define MOCPP_PLATFORM MOCPP_PLATFORM_ARDUINO
#endif

#if MOCPP_PLATFORM == MOCPP_PLATFORM_NONE
#ifndef MOCPP_CUSTOM_CONSOLE
#define MOCPP_CUSTOM_CONSOLE
#endif
#ifndef MOCPP_CUSTOM_TIMER
#define MOCPP_CUSTOM_TIMER
#endif
#endif

#ifdef MOCPP_CUSTOM_CONSOLE
#include <cstdio>

#ifndef MOCPP_CUSTOM_CONSOLE_MAXMSGSIZE
#define MOCPP_CUSTOM_CONSOLE_MAXMSGSIZE 192
#endif

void mo_set_console_out(void (*console_out)(const char *msg));

namespace MicroOcpp {
void mocpp_console_out(const char *msg);
}
#define MOCPP_CONSOLE_PRINTF(X, ...) \
            do { \
                char msg [MOCPP_CUSTOM_CONSOLE_MAXMSGSIZE]; \
                auto _mo_ret = snprintf(msg, MOCPP_CUSTOM_CONSOLE_MAXMSGSIZE, X, ##__VA_ARGS__); \
                if (_mo_ret < 0 || _mo_ret >= MOCPP_CUSTOM_CONSOLE_MAXMSGSIZE) { \
                    sprintf(msg + MOCPP_CUSTOM_CONSOLE_MAXMSGSIZE - 7, " [...]"); \
                } \
                MicroOcpp::mocpp_console_out(msg); \
            } while (0)
#else
#define mo_set_console_out(X) \
            do { \
                X("[OCPP] CONSOLE ERROR: mo_set_console_out ignored if MOCPP_CUSTOM_CONSOLE " \
                  "not defined\n"); \
                char msg [196]; \
                snprintf(msg, 196, "     > see %s:%i",__FILE__,__LINE__); \
                X(msg); \
                X("\n     > see MicroOcpp/Platform.h\n"); \
            } while (0)

#if MOCPP_PLATFORM == MOCPP_PLATFORM_ARDUINO
#include <Arduino.h>
#ifndef MOCPP_USE_SERIAL
#define MOCPP_USE_SERIAL Serial
#endif

#define MOCPP_CONSOLE_PRINTF(X, ...) MOCPP_USE_SERIAL.printf_P(PSTR(X), ##__VA_ARGS__)
#elif MOCPP_PLATFORM == MOCPP_PLATFORM_ESPIDF || MOCPP_PLATFORM == MOCPP_PLATFORM_UNIX
#include <stdio.h>

#define MOCPP_CONSOLE_PRINTF(X, ...) printf(X, ##__VA_ARGS__)
#endif
#endif

#ifdef MOCPP_CUSTOM_TIMER
void mocpp_set_timer(unsigned long (*get_ms)());

unsigned long mocpp_tick_ms_custom();
#define mocpp_tick_ms mocpp_tick_ms_custom
#else

#if MOCPP_PLATFORM == MOCPP_PLATFORM_ARDUINO
#include <Arduino.h>
#define mocpp_tick_ms millis
#elif MOCPP_PLATFORM == MOCPP_PLATFORM_ESPIDF
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define mocpp_tick_ms(X) ((xTaskGetTickCount() * 1000UL) / configTICK_RATE_HZ)
#elif MOCPP_PLATFORM == MOCPP_PLATFORM_UNIX
unsigned long mocpp_tick_ms_unix();
#define mocpp_tick_ms mocpp_tick_ms_unix
#endif
#endif

#ifndef MOCPP_MAX_JSON_CAPACITY
#if MOCPP_PLATFORM == MOCPP_PLATFORM_UNIX
#define MOCPP_MAX_JSON_CAPACITY 16384
#else
#define MOCPP_MAX_JSON_CAPACITY 4096
#endif
#endif

#if MOCPP_PLATFORM != MOCPP_PLATFORM_ARDUINO
void dtostrf(float value, int min_width, int num_digits_after_decimal, char *target);
#endif

#endif
