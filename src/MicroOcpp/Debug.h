// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef MOCPP_DEBUG_H
#define MOCPP_DEBUG_H

#include <MicroOcpp/Platform.h>

#define MOCPP_DL_NONE 0x00     //suppress all output to the console
#define MOCPP_DL_ERROR 0x01    //report failures
#define MOCPP_DL_WARN 0x02     //report observed or assumed inconsistent state
#define MOCPP_DL_INFO 0x03     //make internal state apparent
#define MOCPP_DL_DEBUG 0x04    //relevant info for debugging
#define MOCPP_DL_VERBOSE 0x05  //all output

#ifndef MOCPP_DBG_LEVEL
#define MOCPP_DBG_LEVEL MOCPP_DL_INFO  //default
#endif

#define MOCPP_DF_MINIMAL 0x00   //don't reveal origin of a debug message
#define MOCPP_DF_MODULE_LINE 0x01 //print module by file name and line number
#define MOCPP_DF_FILE_LINE 0x02 //print file and line number
#define MOCPP_DF_FULL    0x03   //print path and file and line numbr

#ifndef MOCPP_DBG_FORMAT
#define MOCPP_DBG_FORMAT MOCPP_DF_FILE_LINE //default
#endif


#if MOCPP_DBG_FORMAT == MOCPP_DF_MINIMAL
#define MOCPP_DBG(level, X)   \
    do {                        \
        MOCPP_CONSOLE_PRINTF("[OCPP] ");           \
        MOCPP_CONSOLE_PRINTF X;  \
        MOCPP_CONSOLE_PRINTF("\n");         \
    } while (0);

#elif MOCPP_DBG_FORMAT == MOCPP_DF_MODULE_LINE
#define MOCPP_DBG(level, X)   \
    do {                        \
        const char *_mo_file = __FILE__;         \
        size_t _mo_l = sizeof(__FILE__);         \
        size_t _mo_r = _mo_l;         \
        while (_mo_l > 0 && _mo_file[_mo_l-1] != '/' && _mo_file[_mo_l-1] != '\\') {         \
            _mo_l--;         \
            if (_mo_file[_mo_l] == '.') _mo_r = _mo_l;         \
        }         \
        MOCPP_CONSOLE_PRINTF("[OCPP] %s (%.*s:%i): ",level, (int) (_mo_r - _mo_l), _mo_file + _mo_l,__LINE__);           \
        MOCPP_CONSOLE_PRINTF X;  \
        MOCPP_CONSOLE_PRINTF("\n");         \
    } while (0);

#elif MOCPP_DBG_FORMAT == MOCPP_DF_FILE_LINE
#define MOCPP_DBG(level, X)   \
    do {                        \
        const char *_mo_file = __FILE__;         \
        size_t _mo_l = sizeof(__FILE__);         \
        for (; _mo_l > 0 && _mo_file[_mo_l-1] != '/' && _mo_file[_mo_l-1] != '\\'; _mo_l--);         \
        MOCPP_CONSOLE_PRINTF("[OCPP] %s (%s:%i): ",level, _mo_file + _mo_l,__LINE__);           \
        MOCPP_CONSOLE_PRINTF X;  \
        MOCPP_CONSOLE_PRINTF("\n");         \
    } while (0);

#elif MOCPP_DBG_FORMAT == MOCPP_DF_FULL
#define MOCPP_DBG(level, X)   \
    do {                        \
        MOCPP_CONSOLE_PRINTF("[OCPP] %s (%s:%i): ",level, __FILE__,__LINE__);           \
        MOCPP_CONSOLE_PRINTF X;  \
        MOCPP_CONSOLE_PRINTF("\n");         \
    } while (0);

#else
#error invalid MOCPP_DBG_FORMAT definition
#endif


#if MOCPP_DBG_LEVEL >= MOCPP_DL_ERROR
#define MOCPP_DBG_ERR(...) MOCPP_DBG("ERROR",(__VA_ARGS__))
#else
#define MOCPP_DBG_ERR(...)
#endif

#if MOCPP_DBG_LEVEL >= MOCPP_DL_WARN
#define MOCPP_DBG_WARN(...) MOCPP_DBG("warning",(__VA_ARGS__))
#else
#define MOCPP_DBG_WARN(...)
#endif

#if MOCPP_DBG_LEVEL >= MOCPP_DL_INFO
#define MOCPP_DBG_INFO(...) MOCPP_DBG("info",(__VA_ARGS__))
#else
#define MOCPP_DBG_INFO(...)
#endif

#if MOCPP_DBG_LEVEL >= MOCPP_DL_DEBUG
#define MOCPP_DBG_DEBUG(...) MOCPP_DBG("debug",(__VA_ARGS__))
#else
#define MOCPP_DBG_DEBUG(...)
#endif

#if MOCPP_DBG_LEVEL >= MOCPP_DL_VERBOSE
#define MOCPP_DBG_VERBOSE(...) MOCPP_DBG("verbose",(__VA_ARGS__))
#else
#define MOCPP_DBG_VERBOSE(...)
#endif

#ifdef MOCPP_TRAFFIC_OUT

#define MOCPP_DBG_TRAFFIC_OUT(...)   \
    do {                        \
        MOCPP_CONSOLE_PRINTF("[OCPP] Send: %s",__VA_ARGS__);           \
        MOCPP_CONSOLE_PRINTF("\n");         \
    } while (0)

#define MOCPP_DBG_TRAFFIC_IN(...)   \
    do {                        \
        MOCPP_CONSOLE_PRINTF("[OCPP] Recv: %.*s",__VA_ARGS__);           \
        MOCPP_CONSOLE_PRINTF("\n");         \
    } while (0)

#else
#define MOCPP_DBG_TRAFFIC_OUT(...)
#define MOCPP_DBG_TRAFFIC_IN(...)
#endif

#endif
