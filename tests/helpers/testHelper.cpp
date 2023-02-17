#include <iostream>
#include <ArduinoOcpp.h>

using namespace ArduinoOcpp;

void cpp_console_out(const char *msg) {
    std::cout << msg;
}

unsigned long mtime = 10000;
unsigned long custom_timer_cb() {
    return mtime;
}

void loop() {
    for (int i = 0; i < 30; i++) {
        mtime += 100;
        OCPP_loop();
    }
}
