// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <catch2/catch.hpp>
#include <catch2/catch.hpp>

#include <MicroOcpp.h>
#include <MicroOcpp/Core/Memory.h>

using namespace MicroOcpp;

uint32_t mtime = 10000;
uint32_t custom_timer_cb() {
    return mtime;
}

void loop() {
    for (int i = 0; i < 30; i++) {
        mtime += 100;
        mo_loop();
    }
}

class TestRunListener : public Catch::TestEventListenerBase {
public:
    using Catch::TestEventListenerBase::TestEventListenerBase;

    void testRunEnded( Catch::TestRunStats const& testRunStats ) override {
        MO_MEM_PRINT_STATS();
        MO_MEM_DEINIT();
    }
};

CATCH_REGISTER_LISTENER(TestRunListener)
