// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
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
#define MO_EXTERN_C extern "C"
#else
#define MO_EXTERN_C
#endif

#if MO_PLATFORM == MO_PLATFORM_NONE
#ifndef MO_CUSTOM_CONSOLE
#define MO_CUSTOM_CONSOLE
#endif
#ifndef MO_CUSTOM_TIMER
#define MO_CUSTOM_TIMER
#endif
#endif

#ifdef MO_CUSTOM_CONSOLE
#include <stdio.h>

#ifndef MO_CUSTOM_CONSOLE_MAXMSGSIZE
#define MO_CUSTOM_CONSOLE_MAXMSGSIZE 256
#endif

extern char _mo_console_msg_buf [MO_CUSTOM_CONSOLE_MAXMSGSIZE]; //define msg_buf in data section to save memory (see https://github.com/matth-x/MicroOcpp/pull/304)
MO_EXTERN_C void _mo_console_out(const char *msg);

MO_EXTERN_C void mocpp_set_console_out(void (*console_out)(const char *msg));

#define MO_CONSOLE_PRINTF(X, ...) \
            do { \
                auto _mo_ret = snprintf(_mo_console_msg_buf, MO_CUSTOM_CONSOLE_MAXMSGSIZE, X, ##__VA_ARGS__); \
                if (_mo_ret < 0 || _mo_ret >= MO_CUSTOM_CONSOLE_MAXMSGSIZE) { \
                    sprintf(_mo_console_msg_buf + MO_CUSTOM_CONSOLE_MAXMSGSIZE - 7, " [...]"); \
                } \
                _mo_console_out(_mo_console_msg_buf); \
            } while (0)
#else
#define mocpp_set_console_out(X) \
            do { \
                X("[OCPP] CONSOLE ERROR: mocpp_set_console_out ignored if MO_CUSTOM_CONSOLE " \
                  "not defined\n"); \
                char msg [196]; \
                snprintf(msg, 196, "     > see %s:%i",__FILE__,__LINE__); \
                X(msg); \
                X("\n     > see MicroOcpp/Platform.h\n"); \
            } while (0)

#if MO_PLATFORM == MO_PLATFORM_ARDUINO
#include <Arduino.h>
#ifndef MO_USE_SERIAL
#define MO_USE_SERIAL Serial
#endif

#define MO_CONSOLE_PRINTF(X, ...) MO_USE_SERIAL.printf_P(PSTR(X), ##__VA_ARGS__)
#elif MO_PLATFORM == MO_PLATFORM_ESPIDF || MO_PLATFORM == MO_PLATFORM_UNIX
#include <stdio.h>

#define MO_CONSOLE_PRINTF(X, ...) printf(X, ##__VA_ARGS__)
#endif
#endif

#ifdef MO_CUSTOM_TIMER
MO_EXTERN_C void mocpp_set_timer(unsigned long (*get_ms)());

MO_EXTERN_C unsigned long mocpp_tick_ms_custom();
#define mocpp_tick_ms mocpp_tick_ms_custom
#else

#if MO_PLATFORM == MO_PLATFORM_ARDUINO
#include <Arduino.h>
#define mocpp_tick_ms millis
#elif MO_PLATFORM == MO_PLATFORM_ESPIDF
MO_EXTERN_C unsigned long mocpp_tick_ms_espidf();
#define mocpp_tick_ms mocpp_tick_ms_espidf
#elif MO_PLATFORM == MO_PLATFORM_UNIX
MO_EXTERN_C unsigned long mocpp_tick_ms_unix();
#define mocpp_tick_ms mocpp_tick_ms_unix
#endif
#endif

#ifdef MO_CUSTOM_RNG
MO_EXTERN_C void mocpp_set_rng(uint32_t (*rng)());
MO_EXTERN_C uint32_t mocpp_rng_custom();
#define mocpp_rng mocpp_rng_custom
#else
MO_EXTERN_C uint32_t mocpp_time_based_prng(void);
#define mocpp_rng mocpp_time_based_prng
#endif

#ifndef MO_MAX_JSON_CAPACITY
#if MO_PLATFORM == MO_PLATFORM_UNIX
#define MO_MAX_JSON_CAPACITY 16384
#else
#define MO_MAX_JSON_CAPACITY 4096
#endif
#endif

#ifndef MO_ENABLE_MBEDTLS
#define MO_ENABLE_MBEDTLS 0
#endif

#endif
