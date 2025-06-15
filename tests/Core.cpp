// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include <catch2/catch.hpp>
#include "./helpers/testHelper.h"

TEST_CASE( "Time" ) {
    printf("\nRun %s\n",  "Time");

    //initialize Context without any configs
    mo_initialize();

    mtime = 0;
    mo_getContext()->setTicksCb(custom_timer_cb);

    MicroOcpp::LoopbackConnection loopback;
    mo_getContext()->setConnection(&loopback);

    mo_setup();

    auto& clock = mo_getContext()->getClock();

    SECTION("increment system time") {

        auto t0 = clock.now();
        auto uptime0 = clock.getUptime();
    
        mo_loop();

        mtime = 200; //200 ms

        mo_loop();

        auto t200 = clock.now();
        auto uptime200 = clock.getUptime();

        int32_t dt;
        REQUIRE( clock.delta(t200, t0, dt) );
        REQUIRE( dt == 0 ); //floored to 0s
        REQUIRE( clock.delta(uptime200, uptime0, dt) );
        REQUIRE( dt == 0 ); //floored to 0s

        mtime = 1000; //1s

        mo_loop();

        auto t1 = clock.now();
        auto uptime1 = clock.getUptime();

        REQUIRE( clock.delta(t1, t0, dt) );
        REQUIRE( dt == 1 );
        REQUIRE( clock.delta(uptime1, uptime0, dt) );
        REQUIRE( dt == 1 );

        mtime = 3000; //3s, skipped 2s step

        mo_loop();

        auto t3 = clock.now();
        auto uptime3 = clock.getUptime();

        REQUIRE( clock.delta(t3, t0, dt) );
        REQUIRE( dt == 3 );
        REQUIRE( clock.delta(t3, t1, dt) );
        REQUIRE( dt == 2 );
        REQUIRE( clock.delta(uptime3, uptime0, dt) );
        REQUIRE( dt == 3 );
        REQUIRE( clock.delta(uptime3, uptime1, dt) );
        REQUIRE( dt == 2 );

        //system time rolls over, need two steps because clock works with signed values internally
        mtime = (std::numeric_limits<uint32_t>::max() - 999UL) / 2UL;
        mo_loop();
        mtime = std::numeric_limits<uint32_t>::max() - 999UL;
        mo_loop();

        auto tRoll_min1 = clock.now();
        auto uptimeRoll_min1 = clock.getUptime();

        mtime = 0;
        mo_loop();

        auto tRoll_0 = clock.now();
        auto uptimeRoll_0 = clock.getUptime();

        REQUIRE( clock.delta(tRoll_0, tRoll_min1, dt) );
        REQUIRE( dt == 1 );
        REQUIRE( clock.delta(tRoll_0, t0, dt) );
        REQUIRE( dt == (int32_t)(std::numeric_limits<uint32_t>::max() / 1000UL) );
        REQUIRE( clock.delta(uptimeRoll_0, uptimeRoll_min1, dt) );
        REQUIRE( dt == 1 );
        REQUIRE( clock.delta(uptimeRoll_0, uptime0, dt) );
        REQUIRE( dt == (int32_t)(std::numeric_limits<uint32_t>::max() / 1000UL) );

        //system time rolls over too early
        mtime = std::numeric_limits<uint32_t>::max() / 100UL - 999UL; //rollover from ~0.5 days to 0 instead of ~50 days to 0
        mo_loop();

        tRoll_min1 = clock.now();
        uptimeRoll_min1 = clock.getUptime();

        mtime = 0;
        mo_loop();

        tRoll_0 = clock.now();
        uptimeRoll_0 = clock.getUptime();

        REQUIRE( clock.delta(tRoll_0, tRoll_min1, dt) );
        REQUIRE( dt == 0 ); //value corrected to 0
        REQUIRE( clock.delta(uptimeRoll_0, uptimeRoll_min1, dt) );
        REQUIRE( dt == 0 );
    }



    mo_deinitialize();
}
