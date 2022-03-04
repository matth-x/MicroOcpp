// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef AO_PLATFORM_H
#define AO_PLATFORM_H

#ifdef AO_CUSTOM_CONSOLE

#ifndef AO_CUSTOM_CONSOLE_MAXMSGSIZE
#define AO_CUSTOM_CONSOLE_MAXMSGSIZE 128
#endif

void ao_set_console_out(void (*console_out)(const char *msg));

namespace ArduinoOcpp {
void ao_console_out(const char *msg);
}
#define AO_CONSOLE_PRINTF(X, ...) \
            do { \
                char msg [AO_CUSTOM_CONSOLE_MAXMSGSIZE]; \
                snprintf(msg, AO_CUSTOM_CONSOLE_MAXMSGSIZE, X, ##__VA_ARGS__); \
                sprintf(msg + AO_CUSTOM_CONSOLE_MAXMSGSIZE - 7, " [...]"); \
                ao_console_out(msg); \
            } while (0)
#else
#define ao_set_console_out(X) \
            do { \
                X("[AO] CONSOLE ERROR: ao_set_console_out ignored if AO_CUSTOM_CONSOLE " \
                  "not defined\n"); \
                char msg [100]; \
                snprintf(msg, 100, "     > see %s:%i",__FILE__,__LINE__); \
                X(msg); \
                X("\n     > see ArduinoOcpp/Platform.h\n"); \
            } while (0)
#endif

#ifndef AO_CONSOLE_PRINTF
//begin with Arduino support, add more later
#include <Arduino.h>
#ifndef AO_USE_SERIAL
#define AO_USE_SERIAL Serial
#endif

#define AO_CONSOLE_PRINTF(X, ...) AO_USE_SERIAL.printf_P(PSTR(X), ##__VA_ARGS__)
#endif

#ifndef ao_tick_ms
#include <Arduino.h>
#define ao_tick_ms millis
#endif

#ifndef ao_avail_heap
#include <Arduino.h>
#define ao_avail_heap ESP.getFreeHeap
#endif

#endif
