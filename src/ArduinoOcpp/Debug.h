// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef AO_DEBUG_H
#define AO_DEBUG_H

#include <ArduinoOcpp/Platform.h>

#define AO_DL_NONE 0x00     //suppress all output to the console
#define AO_DL_ERROR 0x01    //report failures
#define AO_DL_WARN 0x02     //report observed or assumed inconsistent state
#define AO_DL_INFO 0x03     //make internal state apparent
#define AO_DL_DEBUG 0x04    //relevant info for debugging
#define AO_DL_VERBOSE 0x05  //all output

#ifndef AO_DBG_LEVEL
#define AO_DBG_LEVEL AO_DL_INFO  //default
#endif

#define AO_DBG(level, X)   \
    do {                        \
        AO_CONSOLE_PRINTF("[AO] %s (%s:%i): ",level, __FILE__,__LINE__);           \
        AO_CONSOLE_PRINTF X;  \
        AO_CONSOLE_PRINTF("\n");         \
    } while (0);

#if AO_DBG_LEVEL >= AO_DL_ERROR
#define AO_DBG_ERR(...) AO_DBG("ERROR",(__VA_ARGS__))
#else
#define AO_DBG_ERR(...)
#endif

#if AO_DBG_LEVEL >= AO_DL_WARN
#define AO_DBG_WARN(...) AO_DBG("warning",(__VA_ARGS__))
#else
#define AO_DBG_WARN(...)
#endif

#if AO_DBG_LEVEL >= AO_DL_INFO
#define AO_DBG_INFO(...) AO_DBG("info",(__VA_ARGS__))
#else
#define AO_DBG_INFO(...)
#endif

#if AO_DBG_LEVEL >= AO_DL_DEBUG
#define AO_DBG_DEBUG(...) AO_DBG("debug",(__VA_ARGS__))
#else
#define AO_DBG_DEBUG(...)
#endif

#if AO_DBG_LEVEL >= AO_DL_VERBOSE
#define AO_DBG_VERBOSE(...) AO_DBG("verbose",(__VA_ARGS__))
#else
#define AO_DBG_VERBOSE(...)
#endif

#ifdef AO_TRAFFIC_OUT
#define AO_DBG_TRAFFIC_OUT(...)  AO_CONSOLE_PRINTF("[AO] To WS lib: %s\n",__VA_ARGS__)
#define AO_DBG_TRAFFIC_IN(...)  AO_CONSOLE_PRINTF("[AO] From WS lib: %s\n",__VA_ARGS__)
#else
#define AO_DBG_TRAFFIC_OUT(...)
#define AO_DBG_TRAFFIC_IN(...)
#endif

#endif
