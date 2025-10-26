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
#define MO_DBG_LEVEL_MBEDTLS 1 //Error
#endif

#ifndef MO_DBG_ENDL
#define MO_DBG_ENDL "\n"
#endif

#ifndef MO_DBG_MAXMSGSIZE
#ifdef MO_CUSTOM_CONSOLE_MAXMSGSIZE
#define MO_DBG_MAXMSGSIZE MO_CUSTOM_CONSOLE_MAXMSGSIZE
#else
#define MO_DBG_MAXMSGSIZE 256
#endif
#endif //MO_DBG_MAXMSGSIZE

#ifdef __cplusplus

namespace MicroOcpp {
class Debug {
private:
    void (*debugCb)(const char *msg) = nullptr;
    void (*debugCb2)(int lvl, const char *fn, int line, const char *msg) = nullptr;

    char buf [MO_DBG_MAXMSGSIZE] = {'\0'};
    int dbgLevel = MO_DBG_LEVEL;
public:
    Debug() = default;

    void setDebugCb(void (*debugCb)(const char *msg));
    void setDebugCb2(void (*debugCb2)(int lvl, const char *fn, int line, const char *msg));

    void setDebugLevel(int dbgLevel);

    bool setup();

    void operator()(int lvl, const char *fn, int line, const char *format, ...);
};

extern Debug debug;
} //namespace MicroOcpp
#endif //__cplusplus

#if MO_DBG_LEVEL >= MO_DL_ERROR
#define MO_DBG_ERR(...) MicroOcpp::debug(MO_DL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#else
#define MO_DBG_ERR(...) (void)0
#endif

#if MO_DBG_LEVEL >= MO_DL_WARN
#define MO_DBG_WARN(...) MicroOcpp::debug(MO_DL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#else
#define MO_DBG_WARN(...) (void)0
#endif

#if MO_DBG_LEVEL >= MO_DL_INFO
#define MO_DBG_INFO(...) MicroOcpp::debug(MO_DL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#else
#define MO_DBG_INFO(...) (void)0
#endif

#if MO_DBG_LEVEL >= MO_DL_DEBUG
#define MO_DBG_DEBUG(...) MicroOcpp::debug(MO_DL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#else
#define MO_DBG_DEBUG(...) (void)0
#endif

#if MO_DBG_LEVEL >= MO_DL_VERBOSE
#define MO_DBG_VERBOSE(...) MicroOcpp::debug(MO_DL_VERBOSE, __FILE__, __LINE__, __VA_ARGS__)
#else
#define MO_DBG_VERBOSE(...) (void)0
#endif

#endif
