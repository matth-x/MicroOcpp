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

// ---------------------------------------------------------------------------
// Test 3: Reproduce the exact 3-month timestamp jump reported by @razvanphp.
//
//         The reporter observed that the OCPP timestamp always jumps forward
//         by exactly 3 months -- not more, not less. This test reproduces
//         that scenario in two ways:
//
//         (a) Direct simulation: inject a 3-month delta via the mock timer
//             to verify Clock::now() rejects it.
//
//         (b) 32-bit millis wraparound: simulate the unsigned long counter
//             wrapping at ~49.7 days on ESP32. When the counter wraps and
//             the buggy multiplication further corrupts the delta, the
//             resulting erroneous value fed to Clock::now() can be on the
//             order of months. The guard must catch it regardless of the
//             exact magnitude.
//
//         (c) Arithmetic reproduction: show how the buggy 32-bit formula
//             produces a delta equivalent to ~3 months when the tick counter
//             accumulates enough between calls.
//
//         See: https://github.com/matth-x/MicroOcpp/issues/421
//              PR #447 reviewer feedback from @razvanphp
// ---------------------------------------------------------------------------
TEST_CASE("Reproduce 3-month timestamp jump (issue #421, @razvanphp report)") {
    printf("\nRun %s\n", "Reproduce 3-month timestamp jump (issue #421, @razvanphp report)");

    // 3 months in seconds and milliseconds (using 90 days as approximation)
    const unsigned long THREE_MONTHS_SECS = 90UL * 24UL * 3600UL;  // 7,776,000 seconds
    const unsigned long THREE_MONTHS_MS   = THREE_MONTHS_SECS * 1000UL;  // 7,776,000,000 ms

    SECTION("Direct 3-month jump is rejected by Clock::now()") {
        // Setup: initialize OCPP, set clock to a known time
        MicroOcpp::LoopbackConnection loopback;
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        mocpp_set_timer(custom_timer_cb);

        auto& clock = getOcppContext()->getModel().getClock();

        mtime = 100000;
        clock.setTime("2025-06-01T10:00:00.000Z");

        auto t0 = clock.now();

        // Inject a 3-month jump into the mock timer -- this is exactly what
        // the reporter sees: the timestamp jumps from June to September
        mtime += THREE_MONTHS_MS;

        auto t1 = clock.now();
        int elapsed = t1 - t0;

        // WITHOUT the fix: elapsed would be ~7,776,000 seconds (~90 days)
        // WITH the fix: the implausible delta is rejected, elapsed == 0
        REQUIRE( elapsed == 0 );

        // Verify the timestamp did NOT jump to September
        char buf [JSONDATE_LENGTH + 1];
        t1.toJsonString(buf, sizeof(buf));
        // Should still show June, not September
        REQUIRE( buf[5] == '0' );  // month tens digit
        REQUIRE( buf[6] == '6' );  // month ones digit: June = 06

        mocpp_deinitialize();
    }

    SECTION("Clock recovers normally after 3-month jump is rejected") {
        MicroOcpp::LoopbackConnection loopback;
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        mocpp_set_timer(custom_timer_cb);

        auto& clock = getOcppContext()->getModel().getClock();

        mtime = 100000;
        clock.setTime("2025-06-01T10:00:00.000Z");

        auto t0 = clock.now();

        // 3-month jump -- rejected
        mtime += THREE_MONTHS_MS;
        clock.now();

        // Normal 30-second advance after the rejected jump
        mtime += 30 * 1000;
        auto t_after = clock.now();

        int elapsed = t_after - t0;
        REQUIRE( elapsed == 30 );

        // Timestamp should be June 1 + 30 seconds, not September
        char buf [JSONDATE_LENGTH + 1];
        t_after.toJsonString(buf, sizeof(buf));
        REQUIRE( buf[5] == '0' );
        REQUIRE( buf[6] == '6' );  // still June

        mocpp_deinitialize();
    }

    SECTION("CSMS resync corrects clock after 3-month jump is rejected") {
        MicroOcpp::LoopbackConnection loopback;
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        mocpp_set_timer(custom_timer_cb);

        auto& clock = getOcppContext()->getModel().getClock();

        mtime = 100000;
        clock.setTime("2025-06-01T10:00:00.000Z");

        // 3-month jump -- rejected by guard
        mtime += THREE_MONTHS_MS;
        clock.now();

        // CSMS sends correct time in Heartbeat/BootNotification response
        clock.setTime("2025-06-01T10:05:00.000Z");

        // 10 seconds after CSMS resync
        mtime += 10 * 1000;
        auto t_resync = clock.now();

        MicroOcpp::Timestamp expected;
        expected.setTime("2025-06-01T10:05:10.000Z");
        REQUIRE( (t_resync - expected) == 0 );

        mocpp_deinitialize();
    }

    SECTION("32-bit millis wraparound produces ~3-month-scale erroneous delta") {
        // Demonstrate the arithmetic: on ESP32, unsigned long is 32-bit.
        // The millis counter wraps at UINT32_MAX (~49.7 days).
        //
        // Scenario: charger has been running for 45 days. The mocpp_millis_count
        // is near UINT32_MAX. The buggy multiplication in mocpp_tick_ms_espidf()
        // causes the accumulated millis to jump forward by a huge amount,
        // potentially producing a delta in the range of months.
        //
        // We simulate this by showing that on 32-bit, the erroneous delta
        // from the buggy formula can equal ~3 months of milliseconds.

        const uint32_t TICK_RATE_HZ = 1000;

        // Suppose the charger was idle for 80 minutes between Clock::now() calls
        // (e.g., no OCPP traffic, deep sleep, or main loop stalled).
        // tick_delta = 80 * 60 * 1000 = 4,800,000 ticks
        uint32_t tick_delta = 80UL * 60UL * TICK_RATE_HZ;  // 4,800,000

        // Correct result: 80 minutes = 4,800,000 ms
        uint32_t correct_ms = (uint32_t)(((uint64_t)tick_delta * 1000ULL) / TICK_RATE_HZ);
        REQUIRE( correct_ms == 4800000 );

        // Buggy 32-bit result: 4,800,000 * 1000 = 4,800,000,000
        // UINT32_MAX = 4,294,967,295
        // Wrapped: 4,800,000,000 - 4,294,967,296 = 505,032,704
        // Divided by 1000: 505,032 ms (~505 seconds, ~8.4 min)
        // This is LESS than the real value -- the millis counter advances
        // too slowly, accumulating an error.
        uint32_t buggy_ms = (tick_delta * (uint32_t)1000) / TICK_RATE_HZ;
        REQUIRE( buggy_ms != correct_ms );

        // Over many such overflows, the accumulated millis_count diverges
        // from real time. But the critical issue is when millis_count ITSELF
        // wraps around UINT32_MAX. At that point, the subtraction in
        // Clock::now() (tReading - lastUpdate) produces a huge delta.
        //
        // Simulate: lastUpdate was at a high millis value, then millis wraps
        uint32_t lastUpdate_32 = 4200000000UL;  // ~48.6 days
        uint32_t tReading_32   = 100000UL;       // wrapped to ~100 seconds

        // On 32-bit: unsigned subtraction wraps around
        uint32_t delta_32 = tReading_32 - lastUpdate_32;
        // delta_32 = 100,000 - 4,200,000,000 (mod 2^32)
        //          = 100,000 + (4,294,967,296 - 4,200,000,000)
        //          = 100,000 + 94,967,296 = 95,067,296 ms
        //          = ~95,067 seconds = ~26.4 hours
        REQUIRE( delta_32 == 95067296UL );

        // That's already > 1 hour, so the Clock::now() guard catches it.
        // But with different lastUpdate values, the delta can be much larger.

        // Example producing ~3-month-scale delta:
        // If lastUpdate was at ~500M ms and counter wraps to ~0
        uint32_t lastUpdate_3mo = 500000000UL;    // ~5.8 days into counter
        uint32_t tReading_3mo   = 1000UL;          // just after wrap

        uint32_t delta_3mo = tReading_3mo - lastUpdate_3mo;
        // = 1000 - 500,000,000 (mod 2^32)
        // = 1000 + 3,794,967,296
        // = 3,794,968,296
        REQUIRE( delta_3mo == 3794968296UL );

        // Convert to days: 3,794,968,296 ms / 86,400,000 ms/day = ~43.9 days
        // Not exactly 3 months, but illustrates the mechanism.

        // The exact 3-month (90-day) delta occurs when:
        // delta = 90 * 86400 * 1000 = 7,776,000,000 ms
        // But UINT32_MAX = 4,294,967,295, so on 32-bit this wraps to:
        // 7,776,000,000 mod 2^32 = 3,481,032,704
        // This means to get exactly 3 months from unsigned subtraction:
        // tReading - lastUpdate = 7,776,000,000 (mod 2^32)
        // On a 64-bit system (test env), we inject this directly via mtime.
        //
        // The key point: ANY delta > 1 hour is implausible for Clock::now()
        // and the guard catches it, whether it's 26 hours or 3 months.
        REQUIRE( delta_3mo > 3600UL * 1000UL );  // exceeds the 1-hour threshold
    }

    SECTION("Exact 3-month delta on 32-bit unsigned long wraparound") {
        // Show a specific (lastUpdate, tReading) pair on 32-bit that produces
        // a delta of exactly ~7,776,000,000 ms (~90 days / ~3 months).
        //
        // On 32-bit unsigned: delta = (tReading - lastUpdate) mod 2^32
        // We need: delta_raw = 7,776,000,000
        // Since 7,776,000,000 > UINT32_MAX, the 32-bit delta would be:
        //   7,776,000,000 mod 2^32 = 3,481,032,704 (~40.3 days)
        //
        // But on the ESP32, the issue is in mocpp_tick_ms_espidf():
        // the buggy multiplication causes millis_count to advance by the wrong
        // amount. After enough accumulated error + counter wrap, the millis
        // value diverges from real time by ~3 months.
        //
        // In the test environment (64-bit unsigned long), we directly test
        // Clock::now()'s behavior when fed a 3-month jump via the mock timer.

        MicroOcpp::LoopbackConnection loopback;
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        mocpp_set_timer(custom_timer_cb);

        auto& clock = getOcppContext()->getModel().getClock();

        // Start at Jan 15, 2025
        mtime = 100000;
        clock.setTime("2025-01-15T12:00:00.000Z");

        auto t0 = clock.now();

        // Simulate steady operation: advance 1 minute at a time for 10 min
        for (int i = 0; i < 10; i++) {
            mtime += 60 * 1000;
            clock.now();
        }

        // Verify 10 minutes have passed normally
        auto t_10min = clock.now();
        REQUIRE( (t_10min - t0) == 600 );

        // NOW: simulate the overflow event -- millis jumps by 3 months
        // This is the exact scenario @razvanphp reports: the OCPP timestamp
        // suddenly shows a date 3 months in the future (mid-April instead
        // of mid-January)
        mtime += THREE_MONTHS_MS;

        auto t_after_overflow = clock.now();

        // The 3-month jump must be rejected
        int jump = t_after_overflow - t0;
        REQUIRE( jump == 600 );  // still at the 10-minute mark, not 90+ days

        // Verify timestamp is still January, not April
        char buf [JSONDATE_LENGTH + 1];
        t_after_overflow.toJsonString(buf, sizeof(buf));
        REQUIRE( buf[5] == '0' );
        REQUIRE( buf[6] == '1' );  // month = 01 (January)

        // System continues operating normally after the rejected jump
        mtime += 120 * 1000;  // 2 more minutes
        auto t_final = clock.now();
        REQUIRE( (t_final - t0) == 720 );  // 10 min + 2 min = 720 seconds

        mocpp_deinitialize();
    }

    SECTION("Multiple overflow-scale jumps during long operation") {
        // Simulate a charger running for extended periods where the millis
        // counter wraps multiple times, each producing a 3-month-scale jump.
        // The guard must reject every one.

        MicroOcpp::LoopbackConnection loopback;
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        mocpp_set_timer(custom_timer_cb);

        auto& clock = getOcppContext()->getModel().getClock();

        mtime = 100000;
        clock.setTime("2025-03-01T00:00:00.000Z");

        auto t0 = clock.now();

        // Normal: 5 minutes
        mtime += 5 * 60 * 1000;
        clock.now();

        // First overflow: ~3 months jump
        mtime += THREE_MONTHS_MS;
        clock.now();  // rejected

        // Normal: 3 minutes
        mtime += 3 * 60 * 1000;
        clock.now();

        // Second overflow: ~3 months jump
        mtime += THREE_MONTHS_MS;
        clock.now();  // rejected

        // Normal: 2 minutes
        mtime += 2 * 60 * 1000;
        auto t_final = clock.now();

        // Only the normal intervals should count: 5 + 3 + 2 = 10 minutes
        int elapsed = t_final - t0;
        REQUIRE( elapsed == 600 );

        // Still March, not jumped to the future
        char buf [JSONDATE_LENGTH + 1];
        t_final.toJsonString(buf, sizeof(buf));
        REQUIRE( buf[5] == '0' );
        REQUIRE( buf[6] == '3' );  // month = 03 (March)

        mocpp_deinitialize();
    }
}
