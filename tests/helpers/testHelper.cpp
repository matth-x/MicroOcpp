// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <iostream>
#include <MicroOcpp.h>

using namespace MicroOcpp;

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
        mocpp_loop();
    }
}
