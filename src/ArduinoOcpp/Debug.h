// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
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

#define AO_DF_MINIMAL 0x00   //don't reveal origin of a debug message
#define AO_DF_MODULE_LINE 0x01 //print module by file name and line number
#define AO_DF_FILE_LINE 0x02 //print file and line number
#define AO_DF_FULL    0x03   //print path and file and line numbr

#ifndef AO_DBG_FORMAT
#define AO_DBG_FORMAT AO_DF_FILE_LINE //default
#endif


#if AO_DBG_FORMAT == AO_DF_MINIMAL
#define AO_DBG(level, X)   \
    do {                        \
        AO_CONSOLE_PRINTF("[AO] ");           \
        AO_CONSOLE_PRINTF X;  \
        AO_CONSOLE_PRINTF("\n");         \
    } while (0);

#elif AO_DBG_FORMAT == AO_DF_MODULE_LINE
#define AO_DBG(level, X)   \
    do {                        \
        const char *_ao_file = __FILE__;         \
        size_t _ao_l = sizeof(__FILE__);         \
        size_t _ao_r = _ao_l;         \
        while (_ao_l > 0 && _ao_file[_ao_l-1] != '/' && _ao_file[_ao_l-1] != '\\') {         \
            _ao_l--;         \
            if (_ao_file[_ao_l] == '.') _ao_r = _ao_l;         \
        }         \
        AO_CONSOLE_PRINTF("[AO] %s (%.*s:%i): ",level, (int) (_ao_r - _ao_l), _ao_file + _ao_l,__LINE__);           \
        AO_CONSOLE_PRINTF X;  \
        AO_CONSOLE_PRINTF("\n");         \
    } while (0);

#elif AO_DBG_FORMAT == AO_DF_FILE_LINE
#define AO_DBG(level, X)   \
    do {                        \
        const char *_ao_file = __FILE__;         \
        size_t _ao_l = sizeof(__FILE__);         \
        for (; _ao_l > 0 && _ao_file[_ao_l-1] != '/' && _ao_file[_ao_l-1] != '\\'; _ao_l--);         \
        AO_CONSOLE_PRINTF("[AO] %s (%s:%i): ",level, _ao_file + _ao_l,__LINE__);           \
        AO_CONSOLE_PRINTF X;  \
        AO_CONSOLE_PRINTF("\n");         \
    } while (0);

#elif AO_DBG_FORMAT == AO_DF_FULL
#define AO_DBG(level, X)   \
    do {                        \
        AO_CONSOLE_PRINTF("[AO] %s (%s:%i): ",level, __FILE__,__LINE__);           \
        AO_CONSOLE_PRINTF X;  \
        AO_CONSOLE_PRINTF("\n");         \
    } while (0);

#else
#error invalid AO_DBG_FORMAT definition
#endif


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

#define AO_DBG_TRAFFIC_OUT(...)   \
    do {                        \
        AO_CONSOLE_PRINTF("[AO] Send: %s",__VA_ARGS__);           \
        AO_CONSOLE_PRINTF("\n");         \
    } while (0)

#define AO_DBG_TRAFFIC_IN(...)   \
    do {                        \
        AO_CONSOLE_PRINTF("[AO] Recv: %.*s",__VA_ARGS__);           \
        AO_CONSOLE_PRINTF("\n");         \
    } while (0)

#else
#define AO_DBG_TRAFFIC_OUT(...)
#define AO_DBG_TRAFFIC_IN(...)
#endif

#endif
