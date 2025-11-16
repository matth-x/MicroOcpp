// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SECURITY_EVENT

#include <MicroOcpp.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/SecurityEvent/SecurityEventService.h>
#include <MicroOcpp/Operations/SecurityEventNotification.h>
#include <MicroOcpp/Debug.h>
#include <catch2/catch.hpp>
#include "./helpers/testHelper.h"

#include <vector>

#define BASE_TIME_UNIX 1672531200  // 2023-01-01T00:00:00Z
#define BASE_TIME_STRING "2023-01-01T00:00:00Z"

using namespace MicroOcpp;

std::vector<std::pair<std::string, DynamicJsonDocument>> sentSecurityEvents;

void handleSecurityEventRequest(const char *operationType, const char *payloadJson, void **userStatus, void*) {
    if (strcmp(operationType, "SecurityEventNotification") == 0) {
        DynamicJsonDocument doc(1024);
        auto err = deserializeJson(doc, payloadJson);
        if (err) {
            char buf[100];
            snprintf(buf, sizeof(buf), "JSON deserialization error: %s", err.c_str());
            FAIL(buf);
        }
        sentSecurityEvents.emplace_back(operationType, std::move(doc));
    }
}

int writeSecurityEventResponse(const char *operationType, char *buf, size_t size, void *userStatus, void*) {
    // Write empty response for SecurityEventNotification
    if (strcmp(operationType, "SecurityEventNotification") == 0) {
        return snprintf(buf, size, "{}");
    }
    return 0;
}


TEST_CASE("SecurityEventService") {
    printf("\nRun %s\n", "SecurityEventService");

    sentSecurityEvents.clear();

    auto ocppVersion = GENERATE(MO_OCPP_V16, MO_OCPP_V201);

    // Initialize Context
    mo_initialize();
    mo_setTicksCb(custom_timer_cb);

    LoopbackConnection loopback;
    mo_setConnection(&loopback);

    mo_setOcppVersion(ocppVersion);
    mo_setBootNotificationData("TestModel", "TestVendor");

    mo_setup();

    // Capture SecurityEventNotification messages
    mo_setRequestHandler(mo_getApiContext(), "SecurityEventNotification",
                        handleSecurityEventRequest, writeSecurityEventResponse, nullptr, nullptr);

    // Set base time for consistent testing
    mo_setUnixTime(BASE_TIME_UNIX);

    SECTION("Clean up files") {
        auto filesystem = mo_getFilesystem();
        if (filesystem) {
            FilesystemUtils::removeByPrefix(filesystem, "");
        }
    }

    SECTION("Basic Event Triggering - Volatile Mode") {

        // Re-initialize without filesystem to test volatile mode
        mo_deinitialize();
        mo_initialize();
        mo_setTicksCb(custom_timer_cb);
        mo_setConnection(&loopback);
        mo_setOcppVersion(ocppVersion);
        mo_setBootNotificationData("TestModel", "TestVendor");

        // Initialize without filesystem (volatile mode)
        mo_setFilesystemConfig(MO_FS_OPT_DISABLE);

        mo_setup();
        mo_setRequestHandler(mo_getApiContext(), "SecurityEventNotification",
                            handleSecurityEventRequest, writeSecurityEventResponse, nullptr, nullptr);
        mo_setUnixTime(BASE_TIME_UNIX);

        loop();

        auto securityEventService = mo_getContext()->getModelCommon().getSecurityEventService();
        REQUIRE(securityEventService != nullptr);

        sentSecurityEvents.clear();

        // Trigger a security event
        bool result = securityEventService->triggerSecurityEvent("FirmwareUpdated");
        REQUIRE(result == true);

        // Event should be sent immediately in volatile mode
        loop();

        // Verify SecurityEventNotification was sent
        REQUIRE(sentSecurityEvents.size() == 1);
        REQUIRE(sentSecurityEvents[0].first == "SecurityEventNotification");

        auto& eventDoc = sentSecurityEvents[0].second;
        REQUIRE(std::string(eventDoc["type"] | "") == "FirmwareUpdated");
        REQUIRE(strncmp(eventDoc["timestamp"] | "", BASE_TIME_STRING, strlen("2023-01-01")) == 0); // prefix check sufficient

        // Test multiple events
        sentSecurityEvents.clear();

        securityEventService->triggerSecurityEvent("ReconfigurationOfSecurityParameters");
        securityEventService->triggerSecurityEvent("MemoryExhaustion");

        loop();

        // Both events should be sent immediately
        REQUIRE(sentSecurityEvents.size() == 2);
        REQUIRE(std::string(sentSecurityEvents[0].second["type"] | "") == "ReconfigurationOfSecurityParameters");
        REQUIRE(std::string(sentSecurityEvents[1].second["type"] | "") == "MemoryExhaustion");
    }

    SECTION("Time Adjustment Detection") {

        loop();

        auto securityEventService = mo_getContext()->getModelCommon().getSecurityEventService();
        REQUIRE(securityEventService != nullptr);

        // Set time adjustment reporting threshold to 20 seconds (default value)
        mo_setVarConfigInt(mo_getApiContext(), "ClockCtrlr", "TimeAdjustmentReportingThreshold", MO_CONFIG_EXT_PREFIX "TimeAdjustmentReportingThreshold", 20);

        // Initial time setup
        mo_setUnixTime(BASE_TIME_UNIX);
        loop(); // Let the service track the initial time

        sentSecurityEvents.clear();

        // Test 1: Small time adjustment (below threshold) - should NOT trigger event
        mo_setUnixTime(BASE_TIME_UNIX + 10); // +10 seconds (below 20s threshold)
        loop();

        REQUIRE(sentSecurityEvents.size() == 0); // No event should be triggered

        // Test 2: Large positive time adjustment (above threshold) - should trigger event
        sentSecurityEvents.clear();
        mo_setUnixTime(mo_getUnixTime() + 30);// +30 seconds (above 20s threshold)
        loop();

        REQUIRE(sentSecurityEvents.size() == 1);
        REQUIRE(sentSecurityEvents[0].first == "SecurityEventNotification");
        REQUIRE(std::string(sentSecurityEvents[0].second["type"] | "") == "SettingSystemTime");

        // Test 3: Large negative time adjustment (above threshold) - should trigger event
        sentSecurityEvents.clear();
        mo_setUnixTime(BASE_TIME_UNIX - 25); // -25 seconds (above 20s threshold in absolute value)
        loop();

        REQUIRE(sentSecurityEvents.size() == 1);
        REQUIRE(sentSecurityEvents[0].first == "SecurityEventNotification");
        REQUIRE(std::string(sentSecurityEvents[0].second["type"] | "") == "SettingSystemTime");

        // Test 4: Boundary test - exactly at threshold should trigger event
        sentSecurityEvents.clear();
        mo_setUnixTime(BASE_TIME_UNIX + 20); // exactly 20 seconds (at threshold)
        loop();

        REQUIRE(sentSecurityEvents.size() == 1);
        REQUIRE(sentSecurityEvents[0].first == "SecurityEventNotification");
        REQUIRE(std::string(sentSecurityEvents[0].second["type"] | "") == "SettingSystemTime");

        // Test 5: Boundary test - just below threshold should NOT trigger event
        sentSecurityEvents.clear();
        mo_setUnixTime(BASE_TIME_UNIX + 19); // 19 seconds (just below threshold)
        loop();

        REQUIRE(sentSecurityEvents.size() == 0); // No event should be triggered

        // Test 6: Test with threshold set to 0 (disabled) - should NOT trigger events
        mo_setVarConfigInt(mo_getApiContext(), "ClockCtrlr", "TimeAdjustmentReportingThreshold", MO_CONFIG_EXT_PREFIX "TimeAdjustmentReportingThreshold", 0);

        sentSecurityEvents.clear();
        mo_setUnixTime(BASE_TIME_UNIX + 100); // Large adjustment, but threshold is 0 (disabled)
        loop();

        REQUIRE(sentSecurityEvents.size() == 0); // No event should be triggered when threshold is 0
    }

    SECTION("Offline queue") {

        loop();

        auto securityEventService = mo_getContext()->getModelCommon().getSecurityEventService();
        REQUIRE(securityEventService != nullptr);

        // Set time adjustment reporting threshold to 20 seconds (default value)
        mo_setVarConfigInt(mo_getApiContext(), "ClockCtrlr", "TimeAdjustmentReportingThreshold", MO_CONFIG_EXT_PREFIX "TimeAdjustmentReportingThreshold", 20);

        // Initial time setup
        mo_setUnixTime(BASE_TIME_UNIX);
        loop(); // Let the service track the initial time

        sentSecurityEvents.clear();

        // Test 1: Fill queue with 40 elements - all should be sent
        mo_setUnixTime(BASE_TIME_UNIX + 10); // +10 seconds (below 20s threshold)
        loop();

        // Loose connection
        loopback.setConnected(false);

        for (size_t i = 0; i < 40; i++) {
            char buf [100];
            snprintf(buf, sizeof(buf), "SecEvent %zu", i);
            securityEventService->triggerSecurityEvent(buf);
            loop();
        }

        REQUIRE(sentSecurityEvents.size() == 0); // No event sent while offline

        // Re-establish connection
        loopback.setConnected(true);

        // Give some processor time
        for (size_t i = 0; i < 40; i++) {
            loop();
        }

        REQUIRE(sentSecurityEvents.size() == 40); // No event sent while offline

        // Test 2: Trigger 80 elements while sending some of them. Queue gets filled and sent at the same time

        sentSecurityEvents.clear();

        // Loose connection
        loopback.setConnected(false);

        // First 40 elements
        for (size_t i = 0; i < 40; i++) {
            char buf [100];
            snprintf(buf, sizeof(buf), "SecEvent %zu", i);
            securityEventService->triggerSecurityEvent(buf);
            loop();
        }

        REQUIRE(sentSecurityEvents.size() == 0); // No event sent while offline

        // Re-establish connection
        loopback.setConnected(true);

        // Give MO some time to send some elements. 10 iterations should be enough for 5 to 10 elements
        for (size_t i = 0; i < 10; i++) {
            mo_loop();
        }

        REQUIRE(sentSecurityEvents.size() >= 5); // Expect at least 5 elements - otherwise test is not meaningful
        REQUIRE(sentSecurityEvents.size() <= 10); // Expect at least 5 elements - otherwise test is probably broken

        // Next 40 elements
        for (size_t i = 0; i < 40; i++) {
            char buf [100];
            snprintf(buf, sizeof(buf), "SecEvent %i %zu", __LINE__, i);
            securityEventService->triggerSecurityEvent(buf);
            loop();
        }

        // Give some processor time
        for (size_t i = 0; i < 80; i++) {
            loop();
        }

        REQUIRE(sentSecurityEvents.size() == 80); // All 80 events sent

        // Test 3: Max out capacity - err on further events - send all events when online again

        sentSecurityEvents.clear();

        // Loose connection
        loopback.setConnected(false);

        // A SecurityEvent file can hold up to 20 entries. If all outstanding entries are sent, it could
        // be that the latest file contains 19 historic entries and 1 free spot. For calculating the max
        // capacity, it needs to be considered that almost one file could be occupied by historical events.
        // Space that's lost for the offline queue.

        size_t enqueuedSize = 0;

        // The following is safe to send
        for (size_t i = 0; i < (MO_SECLOG_MAX_FILES - 1) * MO_SECLOG_MAX_FILE_ENTRIES; i++) {
            char buf [100];
            snprintf(buf, sizeof(buf), "SecEvent %i %zu", __LINE__, i);
            bool enqueued = securityEventService->triggerSecurityEvent(buf);
            REQUIRE(enqueued);
            enqueuedSize++;
            loop();
        }

        // The following may succeed or fail
        for (size_t i = 0; i < MO_SECLOG_MAX_FILE_ENTRIES; i++) {
            char buf [100];
            snprintf(buf, sizeof(buf), "SecEvent %i %zu", __LINE__, i);
            bool enqueued = securityEventService->triggerSecurityEvent(buf);
            if (enqueued) {
                enqueuedSize++;
            }
            loop();
        }

        // At this point, the queue should be full and the following should fail
        bool enqueued = securityEventService->triggerSecurityEvent("SecEvent buf exceeded");
        REQUIRE(!enqueued);

        MO_DBG_INFO("enqued %zu SecurityEvents", enqueuedSize);

        // Now see if all enqueued elements are actually sent
        loopback.setConnected(true);

        // Give some processor time
        for (size_t i = 0; i < enqueuedSize; i++) {
            loop();
        }

        REQUIRE(sentSecurityEvents.size() == enqueuedSize); // All enqueued events sent
    }

    mo_deinitialize();
}

#else
#warning SecurityEvent unit tests disabled
#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SECURITY_EVENT
