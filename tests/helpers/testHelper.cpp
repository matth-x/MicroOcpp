// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <catch2/catch.hpp>
#include <catch2/catch.hpp>

#include <MicroOcpp.h>
#include <MicroOcpp/Core/Memory.h>

using namespace MicroOcpp;

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

class TestRunListener : public Catch::TestEventListenerBase {
public:
    using Catch::TestEventListenerBase::TestEventListenerBase;

    void testRunEnded( Catch::TestRunStats const& testRunStats ) override {
        MO_MEM_PRINT_STATS();
        MO_MEM_RESET();
    }
};

CATCH_REGISTER_LISTENER(TestRunListener)
