// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Platform.h>

#ifdef AO_CUSTOM_CONSOLE

namespace ArduinoOcpp {
void (*ao_console_out_impl)(const char *msg) = nullptr;
}

void ArduinoOcpp::ao_console_out(const char *msg) {
    if (ao_console_out_impl) {
        ao_console_out_impl(msg);
    }
}

void ao_set_console_out(void (*console_out)(const char *msg)) {
    ArduinoOcpp::ao_console_out_impl = console_out;
    console_out("[AO] console initialized\n");
}

#endif
