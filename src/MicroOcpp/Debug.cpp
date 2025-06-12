// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include <MicroOcpp/Debug.h>

namespace MicroOcpp {
const char *dbgLevelLabel [] = {
    "",         //MO_DL_NONE 0x00
    "ERROR",    //MO_DL_ERROR 0x01
    "warning",  //MO_DL_WARN 0x02
    "info",     //MO_DL_INFO 0x03
    "debug",    //MO_DL_DEBUG 0x04
    "verbose"   //MO_DL_VERBOSE 0x05
};

Debug debug;
} //namespace MicroOcpp

using namespace MicroOcpp;
    
void Debug::setDebugCb(void (*debugCb)(const char *msg)) {
    this->debugCb = debugCb;
}

void Debug::setDebugCb2(void (*debugCb2)(int lvl, const char *fn, int line, const char *msg)) {
    this->debugCb2 = debugCb2;
}

void Debug::setDebugLevel(int dbgLevel) {
    if (dbgLevel > MO_DBG_LEVEL) {
        MO_DBG_ERR("Debug level limited to %s by build config", dbgLevelLabel[MO_DBG_LEVEL]);
        dbgLevel = MO_DBG_LEVEL;
    }
    this->dbgLevel = dbgLevel;
}

bool Debug::setup() {
    if (!debugCb && !debugCb2) {
        // default-initialize console
        debugCb = getDefaultDebugCb(); //defaultDebugCb is null on unsupported platforms. Just inconvenient, not a failure
    }
    return true;
}

void Debug::operator()(int lvl, const char *fn, int line, const char *format, ...) {
    if (debugCb2) {
        va_list args;
        va_start(args, format);
        auto ret = vsnprintf(buf, sizeof(buf), format, args);
        if (ret < 0 || (size_t)ret >= sizeof(buf)) {
            snprintf(buf + sizeof(buf) - sizeof(" [...]"), sizeof(buf), " [...]");
        }
        va_end(args);

        debugCb2(lvl, fn, line, buf);
    } else if (debugCb) {
        size_t l = strlen(fn);
        while (l > 0 && fn[l-1] != '/' && fn[l-1] != '\\') {
            l--;
        }

        auto ret = snprintf(buf, sizeof(buf), "[MO] %s (%s:%i): ", dbgLevelLabel[lvl], fn + l, line);
        if (ret < 0 || (size_t)ret >= sizeof(buf)) {
            snprintf(buf + sizeof(buf) - sizeof(" : "), sizeof(buf), " : ");
        }

        debugCb(buf);

        va_list args;
        va_start(args, format);
        ret = vsnprintf(buf, sizeof(buf), format, args);
        if (ret < 0 || (size_t)ret >= sizeof(buf)) {
            snprintf(buf + sizeof(buf) - sizeof(" [...]"), sizeof(buf), " [...]");
        }
        va_end(args);

        debugCb(buf);
        debugCb(MO_DBG_ENDL);
    }
}
