// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
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

//MbedTLS debug level documented in mbedtls/debug.h:
#ifndef MO_DBG_LEVEL_MBEDTLS
#define MO_DBG_LEVEL_MBEDTLS 1
#endif

#define MO_DF_MINIMAL 0x00   //don't reveal origin of a debug message
#define MO_DF_COMPACT 0x01   //print module by file name and line number
#define MO_DF_FILE_LINE 0x02 //print file and line number
#define MO_DF_FULL 0x03      //print path and file and line numbr

#ifndef MO_DBG_FORMAT
#define MO_DBG_FORMAT MO_DF_FILE_LINE //default
#endif

#ifdef __cplusplus
extern "C" {
#endif

void mo_dbg_print_prefix(int level, const char *fn, int line);
void mo_dbg_print_suffix();

#ifdef __cplusplus
}
#endif

#define MO_DBG(level, X) \
    do { \
        mo_dbg_print_prefix(level, __FILE__, __LINE__); \
        MO_CONSOLE_PRINTF X; \
        mo_dbg_print_suffix(); \
    } while (0)

#if MO_DBG_LEVEL >= MO_DL_ERROR
#define MO_DBG_ERR(...) MO_DBG(MO_DL_ERROR,(__VA_ARGS__))
#else
#define MO_DBG_ERR(...) ((void)0)
#endif

#if MO_DBG_LEVEL >= MO_DL_WARN
#define MO_DBG_WARN(...) MO_DBG(MO_DL_WARN,(__VA_ARGS__))
#else
#define MO_DBG_WARN(...) ((void)0)
#endif

#if MO_DBG_LEVEL >= MO_DL_INFO
#define MO_DBG_INFO(...) MO_DBG(MO_DL_INFO,(__VA_ARGS__))
#else
#define MO_DBG_INFO(...) ((void)0)
#endif

#if MO_DBG_LEVEL >= MO_DL_DEBUG
#define MO_DBG_DEBUG(...) MO_DBG(MO_DL_DEBUG,(__VA_ARGS__))
#else
#define MO_DBG_DEBUG(...) ((void)0)
#endif

#if MO_DBG_LEVEL >= MO_DL_VERBOSE
#define MO_DBG_VERBOSE(...) MO_DBG(MO_DL_VERBOSE,(__VA_ARGS__))
#else
#define MO_DBG_VERBOSE(...) ((void)0)
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
#define MO_DBG_TRAFFIC_OUT(...) ((void)0)
#define MO_DBG_TRAFFIC_IN(...)  ((void)0)
#endif

#endif
