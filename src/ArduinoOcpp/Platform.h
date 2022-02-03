// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef AO_PLATFORM_H
#define AO_PLATFORM_H

#ifndef AO_CONSOLE_PRINTF
//begin with Arduino support, add more later
#include <Arduino.h>
#ifndef AO_USE_SERIAL
#define AO_USE_SERIAL Serial
#endif

#define AO_CONSOLE_PRINTF(X, ...) AO_USE_SERIAL.printf_P(PSTR(X), ##__VA_ARGS__)
#endif

//#ifndef ao_tick_ms
//#include <Arduino.h>
//#define ao_tick_ms millis
//#endif

#endif
