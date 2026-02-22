// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License
//
// Tests for issue #421: Prevent timestamp jumps from tick counter overflow
// https://github.com/matth-x/MicroOcpp/issues/421

#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Core/Time.h>
#include <catch2/catch.hpp>
#include "./helpers/testHelper.h"

#define BASE_TIME "2025-01-15T12:00:00.000Z"

// ---------------------------------------------------------------------------
// Test 1: Reproduce the 32-bit multiplication overflow in the ESP-IDF tick
//         conversion formula (the root cause of ~49,250-second time jumps).
//
//         On ESP32, unsigned long is 32-bit. The buggy formula:
//           millis += ((ticks_now - ticks_last) * 1000UL) / configTICK_RATE_HZ
//         overflows when tick_delta * 1000 > ULONG_MAX.
//
//         This is a pure arithmetic test -- no OCPP context needed.
// ---------------------------------------------------------------------------
TEST_CASE("Tick conversion overflow - 32-bit vs 64-bit arithmetic") {
    printf("\nRun %s\n", "Tick conversion overflow - 32-bit vs 64-bit arithmetic");

    // Simulate ESP32: unsigned long is 32-bit (uint32_t)
    // configTICK_RATE_HZ = 1000 (ESP-IDF default)
    const uint32_t TICK_RATE_HZ = 1000;

    SECTION("Small tick delta - both methods agree") {
        // 60 seconds worth of ticks -- well within 32-bit range
        uint32_t tick_delta = 60 * TICK_RATE_HZ;  // 60,000

        // Buggy 32-bit formula
        uint32_t millis_32bit = (tick_delta * (uint32_t)1000) / TICK_RATE_HZ;

        // Fixed 64-bit formula
        uint32_t millis_64bit = (uint32_t)(((uint64_t)tick_delta * 1000ULL) / TICK_RATE_HZ);

        REQUIRE( millis_32bit == 60000 );
        REQUIRE( millis_64bit == 60000 );
        REQUIRE( millis_32bit == millis_64bit );
    }

    SECTION("Large tick delta - 32-bit overflows, 64-bit correct") {
        // 90 minutes of ticks at 1000 Hz -- exceeds the ~72-minute overflow threshold
        uint32_t tick_delta = 90 * 60 * TICK_RATE_HZ;  // 5,400,000

        // Expected result: 90 minutes = 5,400,000 ms
        uint32_t expected_ms = 90 * 60 * 1000;

        // Buggy 32-bit: tick_delta * 1000 = 5,400,000,000 > UINT32_MAX (4,294,967,295)
        // The intermediate overflows and wraps around
        uint32_t millis_32bit = (tick_delta * (uint32_t)1000) / TICK_RATE_HZ;

        // Fixed 64-bit: intermediate stays in 64-bit, no overflow
        uint32_t millis_64bit = (uint32_t)(((uint64_t)tick_delta * 1000ULL) / TICK_RATE_HZ);

        // The 32-bit result is WRONG due to overflow
        REQUIRE( millis_32bit != expected_ms );

        // The 64-bit result is CORRECT
        REQUIRE( millis_64bit == expected_ms );
    }

    SECTION("Worst case - maximum tick delta") {
        // Maximum possible tick delta (full 32-bit range)
        uint32_t tick_delta = 0xFFFFFFFF;

        // Expected: 0xFFFFFFFF ticks / 1000 Hz * 1000 ms = 0xFFFFFFFF ms = 4,294,967,295 ms
        uint32_t expected_ms = 0xFFFFFFFF;

        // Buggy 32-bit: 0xFFFFFFFF * 1000 truncated to 32-bit
        uint32_t millis_32bit = (tick_delta * (uint32_t)1000) / TICK_RATE_HZ;

        // Fixed 64-bit
        uint32_t millis_64bit = (uint32_t)(((uint64_t)tick_delta * 1000ULL) / TICK_RATE_HZ);

        // 32-bit overflows, produces wrong value
        REQUIRE( millis_32bit != expected_ms );

        // 64-bit is correct
        REQUIRE( millis_64bit == expected_ms );
    }

    SECTION("Boundary - just below and above overflow threshold") {
        // Overflow happens when tick_delta * 1000 > UINT32_MAX
        // Threshold: UINT32_MAX / 1000 = 4,294,967 ticks (~71.6 min at 1000 Hz)
        uint32_t threshold = UINT32_MAX / 1000;

        // Just below threshold -- 32-bit is still correct
        uint32_t tick_delta_safe = threshold;
        uint32_t ms_32bit_safe  = (tick_delta_safe * (uint32_t)1000) / TICK_RATE_HZ;
        uint32_t ms_64bit_safe  = (uint32_t)(((uint64_t)tick_delta_safe * 1000ULL) / TICK_RATE_HZ);
        REQUIRE( ms_32bit_safe == ms_64bit_safe );

        // Just above threshold -- 32-bit overflows
        uint32_t tick_delta_over = threshold + 1;
        uint32_t ms_32bit_over  = (tick_delta_over * (uint32_t)1000) / TICK_RATE_HZ;
        uint32_t ms_64bit_over  = (uint32_t)(((uint64_t)tick_delta_over * 1000ULL) / TICK_RATE_HZ);
        REQUIRE( ms_32bit_over != ms_64bit_over );

        // 64-bit result should be exactly 1 ms more (1 tick = 1 ms at 1000 Hz)
        REQUIRE( ms_64bit_over == ms_64bit_safe + 1 );
    }
}

// ---------------------------------------------------------------------------
// Test 2: Clock::now() guard against implausible time jumps.
//
//         Even if mocpp_tick_ms() produces a large delta (from overflow,
//         concurrency, or any other edge case), Clock::now() should skip
//         the update rather than applying a multi-hour jump.
// ---------------------------------------------------------------------------
TEST_CASE("Clock::now() rejects implausible time deltas") {
    printf("\nRun %s\n", "Clock::now() rejects implausible time deltas");

    // Initialize OCPP engine with loopback connection
    MicroOcpp::LoopbackConnection loopback;
    mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
    mocpp_set_timer(custom_timer_cb);

    auto& clock = getOcppContext()->getModel().getClock();

    // Set initial time
    mtime = 100000;  // 100 seconds into the test
    clock.setTime(BASE_TIME);

    // Read the starting timestamp
    auto t0 = clock.now();
    char t0_str [JSONDATE_LENGTH + 1];
    t0.toJsonString(t0_str, sizeof(t0_str));

    SECTION("Normal time advance works correctly") {
        // Advance by 30 seconds -- well within plausible range
        mtime += 30 * 1000;
        auto t1 = clock.now();

        int elapsed = t1 - t0;
        REQUIRE( elapsed == 30 );
    }

    SECTION("Small accumulated advances work correctly") {
        // Advance in 10-second increments, 6 times = 60 seconds total
        for (int i = 0; i < 6; i++) {
            mtime += 10 * 1000;
            clock.now();
        }

        auto t1 = clock.now();
        int elapsed = t1 - t0;
        REQUIRE( elapsed == 60 );
    }

    SECTION("Implausible jump (> 1 hour) is rejected") {
        // Simulate what happens when mocpp_tick_ms() produces a bogus delta
        // due to overflow: a jump of ~49,250 seconds (~13.7 hours)
        unsigned long overflow_jump_ms = 49250UL * 1000UL;
        mtime += overflow_jump_ms;

        auto t1 = clock.now();

        // The clock should NOT have jumped by ~49,250 seconds.
        // With the fix, the delta is skipped entirely, so elapsed == 0.
        int elapsed = t1 - t0;
        REQUIRE( elapsed == 0 );
    }

    SECTION("Clock recovers after implausible jump is rejected") {
        // First: trigger the implausible jump (gets rejected)
        mtime += 49250UL * 1000UL;
        clock.now();  // rejected, lastUpdate reset to current mtime

        // Now advance by a normal 15 seconds
        mtime += 15 * 1000;
        auto t1 = clock.now();

        // Clock should have advanced by 15 seconds from the original base
        // (the implausible jump was skipped, but the 15s after it is applied)
        int elapsed = t1 - t0;
        REQUIRE( elapsed == 15 );
    }

    SECTION("Jump of exactly 1 hour is accepted") {
        // The threshold is > 1 hour, so exactly 1 hour should still be applied
        mtime += 3600UL * 1000UL;

        auto t1 = clock.now();
        int elapsed = t1 - t0;
        REQUIRE( elapsed == 3600 );
    }

    SECTION("Jump of 1 hour + 1 ms is rejected") {
        // Just over the threshold
        mtime += 3600UL * 1000UL + 1;

        auto t1 = clock.now();
        int elapsed = t1 - t0;
        REQUIRE( elapsed == 0 );
    }

    SECTION("Multiple implausible jumps are all rejected") {
        // First implausible jump
        mtime += 5000UL * 1000UL;
        clock.now();

        // Second implausible jump
        mtime += 8000UL * 1000UL;
        clock.now();

        // Normal advance
        mtime += 10 * 1000;
        auto t1 = clock.now();

        // Only the final 10 seconds should be counted
        int elapsed = t1 - t0;
        REQUIRE( elapsed == 10 );
    }

    SECTION("Clock resync via setTime after implausible jump") {
        // Trigger implausible jump
        mtime += 49250UL * 1000UL;
        clock.now();  // rejected

        // CSMS sends a new time (simulating setTime from server)
        clock.setTime("2025-01-15T14:00:00.000Z");

        // Normal advance of 5 seconds
        mtime += 5 * 1000;
        auto t_after = clock.now();

        // Should be 5 seconds after the new setTime
        char buf [JSONDATE_LENGTH + 1];
        t_after.toJsonString(buf, sizeof(buf));

        MicroOcpp::Timestamp expected;
        expected.setTime("2025-01-15T14:00:05.000Z");
        REQUIRE( (t_after - expected) == 0 );
    }

    mocpp_deinitialize();
}
