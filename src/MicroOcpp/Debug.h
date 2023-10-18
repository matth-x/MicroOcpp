// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef MO_DEBUG_H
#define MO_DEBUG_H

#include <MicroOcpp/Platform.h>

#define MO_DL_NONE 0x00     //suppress all output to the console
#define MO_DL_ERROR 0x01    //report failures
#define MO_DL_WARN 0x02     //report observed or assumed inconsistent state
#define MO_DL_INFO 0x03     //inform about internal state changes
#define MO_DL_DEBUG 0x04    //relevant info for debugging
#define MO_DL_VERBOSE 0x05  //all output

#ifndef MO_DBG_LEVEL
#define MO_DBG_LEVEL MO_DL_INFO  //default
#endif

#define MO_DF_MINIMAL 0x00   //don't reveal origin of a debug message
#define MO_DF_COMPACT 0x01   //print module by file name and line number
#define MO_DF_FILE_LINE 0x02 //print file and line number
#define MO_DF_FULL 0x03      //print path and file and line numbr

#ifndef MO_DBG_FORMAT
#define MO_DBG_FORMAT MO_DF_FILE_LINE //default
#endif


#if MO_DBG_FORMAT == MO_DF_MINIMAL
#define MO_DBG(level, X)   \
    do {                        \
        MO_CONSOLE_PRINTF X;  \
        MO_CONSOLE_PRINTF("\n");         \
    } while (0);

#elif MO_DBG_FORMAT == MO_DF_COMPACT
#define MO_DBG(level, X)   \
    do {                        \
        const char *_mo_file = __FILE__;         \
        size_t _mo_l = sizeof(__FILE__);         \
        size_t _mo_r = _mo_l;         \
        while (_mo_l > 0 && _mo_file[_mo_l-1] != '/' && _mo_file[_mo_l-1] != '\\') {         \
            _mo_l--;         \
            if (_mo_file[_mo_l] == '.') _mo_r = _mo_l;         \
        }         \
        MO_CONSOLE_PRINTF("%.*s:%i ", (int) (_mo_r - _mo_l), _mo_file + _mo_l,__LINE__);           \
        MO_CONSOLE_PRINTF X;  \
        MO_CONSOLE_PRINTF("\n");         \
    } while (0);

#elif MO_DBG_FORMAT == MO_DF_FILE_LINE
#define MO_DBG(level, X)   \
    do {                        \
        const char *_mo_file = __FILE__;         \
        size_t _mo_l = sizeof(__FILE__);         \
        for (; _mo_l > 0 && _mo_file[_mo_l-1] != '/' && _mo_file[_mo_l-1] != '\\'; _mo_l--);         \
        MO_CONSOLE_PRINTF("[MO] %s (%s:%i): ",level, _mo_file + _mo_l,__LINE__);           \
        MO_CONSOLE_PRINTF X;  \
        MO_CONSOLE_PRINTF("\n");         \
    } while (0);

#elif MO_DBG_FORMAT == MO_DF_FULL
#define MO_DBG(level, X)   \
    do {                        \
        MO_CONSOLE_PRINTF("[MO] %s (%s:%i): ",level, __FILE__,__LINE__);           \
        MO_CONSOLE_PRINTF X;  \
        MO_CONSOLE_PRINTF("\n");         \
    } while (0);

#else
#error invalid MO_DBG_FORMAT definition
#endif


#if MO_DBG_LEVEL >= MO_DL_ERROR
#define MO_DBG_ERR(...) MO_DBG("ERROR",(__VA_ARGS__))
#else
#define MO_DBG_ERR(...)
#endif

#if MO_DBG_LEVEL >= MO_DL_WARN
#define MO_DBG_WARN(...) MO_DBG("warning",(__VA_ARGS__))
#else
#define MO_DBG_WARN(...)
#endif

#if MO_DBG_LEVEL >= MO_DL_INFO
#define MO_DBG_INFO(...) MO_DBG("info",(__VA_ARGS__))
#else
#define MO_DBG_INFO(...)
#endif

#if MO_DBG_LEVEL >= MO_DL_DEBUG
#define MO_DBG_DEBUG(...) MO_DBG("debug",(__VA_ARGS__))
#else
#define MO_DBG_DEBUG(...)
#endif

#if MO_DBG_LEVEL >= MO_DL_VERBOSE
#define MO_DBG_VERBOSE(...) MO_DBG("verbose",(__VA_ARGS__))
#else
#define MO_DBG_VERBOSE(...)
#endif

#ifdef MO_TRAFFIC_OUT

#define MO_DBG_TRAFFIC_OUT(...)   \
    do {                        \
        MO_CONSOLE_PRINTF("[MO] Send: %s",__VA_ARGS__);           \
        MO_CONSOLE_PRINTF("\n");         \
    } while (0)

#define MO_DBG_TRAFFIC_IN(...)   \
    do {                        \
        MO_CONSOLE_PRINTF("[MO] Recv: %.*s",__VA_ARGS__);           \
        MO_CONSOLE_PRINTF("\n");         \
    } while (0)

#else
#define MO_DBG_TRAFFIC_OUT(...)
#define MO_DBG_TRAFFIC_IN(...)
#endif

#endif
