#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>
#include <MicroOcpp/Model/SmartCharging/SmartChargingService.h>
#include <MicroOcpp/Operations/CustomOperation.h>
#include "./catch2/catch.hpp"
#include "./helpers/testHelper.h"

#define BASE_TIME "2023-01-01T00:00:00.000Z"

#define SCPROFILE_0                            "[2,\"testmsg\",\"SetChargingProfile\",{\"connectorId\":1,\"csChargingProfiles\":{\"chargingProfileId\":0,\"stackLevel\":0,\"chargingProfilePurpose\":\"TxDefaultProfile\",\"chargingProfileKind\":\"Recurring\",\"recurrencyKind\":\"Daily\",\"validFrom\":\"2022-06-12T00:00:00.000Z\",\"validTo\":\"2023-06-21T00:00:00.000Z\",\"chargingSchedule\":{\"duration\":1000000,\"startSchedule\":\"2023-06-18T00:00:00.000Z\",\"chargingRateUnit\":\"W\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":16,\"numberPhases\":3},{\"startPeriod\":18000,\"limit\":32,\"numberPhases\":3}],\"minChargingRate\":6}}}]"
#define SCPROFILE_0_ALT_SAME_ID                "[2,\"testmsg\",\"SetChargingProfile\",{\"connectorId\":1,\"csChargingProfiles\":{\"chargingProfileId\":0,\"stackLevel\":1,\"chargingProfilePurpose\":\"TxDefaultProfile\",\"chargingProfileKind\":\"Recurring\",\"recurrencyKind\":\"Daily\",\"validFrom\":\"2022-06-12T00:00:00.000Z\",\"validTo\":\"2023-06-21T00:00:00.000Z\",\"chargingSchedule\":{\"duration\":1000000,\"startSchedule\":\"2023-06-18T00:00:00.000Z\",\"chargingRateUnit\":\"W\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":16,\"numberPhases\":3},{\"startPeriod\":18000,\"limit\":32,\"numberPhases\":3}],\"minChargingRate\":6}}}]"
#define SCPROFILE_0_ALT_SAME_STACKEVEL_PURPOSE "[2,\"testmsg\",\"SetChargingProfile\",{\"connectorId\":1,\"csChargingProfiles\":{\"chargingProfileId\":1,\"stackLevel\":0,\"chargingProfilePurpose\":\"TxDefaultProfile\",\"chargingProfileKind\":\"Recurring\",\"recurrencyKind\":\"Daily\",\"validFrom\":\"2022-06-12T00:00:00.000Z\",\"validTo\":\"2023-06-21T00:00:00.000Z\",\"chargingSchedule\":{\"duration\":1000000,\"startSchedule\":\"2023-06-18T00:00:00.000Z\",\"chargingRateUnit\":\"W\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":16,\"numberPhases\":3},{\"startPeriod\":18000,\"limit\":32,\"numberPhases\":3}],\"minChargingRate\":6}}}]"

#define SCPROFILE_1_ABSOLUTE_LIMIT_16A         "[2,\"testmsg\",\"SetChargingProfile\",{\"connectorId\":0,\"csChargingProfiles\":{\"chargingProfileId\":1,\"stackLevel\":0,\"chargingProfilePurpose\":\"ChargePointMaxProfile\",\"chargingProfileKind\":\"Absolute\",\"chargingSchedule\":{\"startSchedule\":\"2023-01-01T00:00:00.000Z\",\"chargingRateUnit\":\"A\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":16,\"numberPhases\":3}]}}}]"

#define SCPROFILE_2_RELATIVE_TXDEF_24A         "[2,\"testmsg\",\"SetChargingProfile\",{\"connectorId\":1,\"csChargingProfiles\":{\"chargingProfileId\":2,\"stackLevel\":0,\"chargingProfilePurpose\":\"TxDefaultProfile\",\"chargingProfileKind\":\"Relative\",\"chargingSchedule\":{\"chargingRateUnit\":\"A\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":24,\"numberPhases\":3}]}}}]"

#define SCPROFILE_3_TXPROF_TXID123_20A         "[2,\"testmsg\",\"SetChargingProfile\",{\"connectorId\":1,\"csChargingProfiles\":{\"chargingProfileId\":3,\"transactionId\":123,\"stackLevel\":0,\"chargingProfilePurpose\":\"TxProfile\",\"chargingProfileKind\":\"Relative\",\"chargingSchedule\":{\"chargingRateUnit\":\"A\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":20,\"numberPhases\":3}]}}}]"

#define SCPROFILE_4_VALID_FROM_2024_16A        "[2,\"testmsg\",\"SetChargingProfile\",{\"connectorId\":0,\"csChargingProfiles\":{\"chargingProfileId\":4,\"stackLevel\":0,\"chargingProfilePurpose\":\"ChargePointMaxProfile\",\"chargingProfileKind\":\"Absolute\",\"validFrom\":\"2024-01-01T00:00:00.000Z\",\"chargingSchedule\":{\"startSchedule\":\"2023-01-01T00:00:00.000Z\",\"chargingRateUnit\":\"A\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":16,\"numberPhases\":3}]}}}]"
#define SCPROFILE_5_VALID_UNTIL_2022_16A       "[2,\"testmsg\",\"SetChargingProfile\",{\"connectorId\":0,\"csChargingProfiles\":{\"chargingProfileId\":5,\"stackLevel\":1,\"chargingProfilePurpose\":\"ChargePointMaxProfile\",\"chargingProfileKind\":\"Absolute\",\"validTo\":  \"2022-01-01T00:00:00.000Z\",\"chargingSchedule\":{\"startSchedule\":\"2023-01-01T00:00:00.000Z\",\"chargingRateUnit\":\"A\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":16,\"numberPhases\":3}]}}}]"

#define SCPROFILE_6_MULTIPLE_PERIODS_16A_20A   "[2,\"testmsg\",\"SetChargingProfile\",{\"connectorId\":0,\"csChargingProfiles\":{\"chargingProfileId\":6,\"stackLevel\":0,\"chargingProfilePurpose\":\"ChargePointMaxProfile\",\"chargingProfileKind\":\"Absolute\",\"chargingSchedule\":{\"startSchedule\":\"2023-01-01T00:00:00.000Z\",\"chargingRateUnit\":\"A\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":16,\"numberPhases\":3},{\"startPeriod\":3600,\"limit\":20,\"numberPhases\":3}]}}}]"

#define SCPROFILE_7_RECURRING_DAY_2H_16A_20A   "[2,\"testmsg\",\"SetChargingProfile\",{\"connectorId\":0,\"csChargingProfiles\":{\"chargingProfileId\":7,\"stackLevel\":0,\"chargingProfilePurpose\":\"ChargePointMaxProfile\",\"chargingProfileKind\":\"Recurring\",\"recurrencyKind\":\"Daily\", \"chargingSchedule\":{\"duration\":7200,\"startSchedule\":\"2023-01-01T00:00:00.000Z\",\"chargingRateUnit\":\"A\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":16,\"numberPhases\":3},{\"startPeriod\":3600,\"limit\":20,\"numberPhases\":3}]}}}]"
#define SCPROFILE_8_RECURRING_WEEK_2H_10A_12A  "[2,\"testmsg\",\"SetChargingProfile\",{\"connectorId\":0,\"csChargingProfiles\":{\"chargingProfileId\":8,\"stackLevel\":1,\"chargingProfilePurpose\":\"ChargePointMaxProfile\",\"chargingProfileKind\":\"Recurring\",\"recurrencyKind\":\"Weekly\",\"chargingSchedule\":{\"duration\":7200,\"startSchedule\":\"2023-01-01T00:00:00.000Z\",\"chargingRateUnit\":\"A\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":10,\"numberPhases\":3},{\"startPeriod\":3600,\"limit\":12,\"numberPhases\":3}]}}}]"

#define SCPROFILE_9_VIA_RMTSTARTTX_20A         "[2,\"testmsg\",\"RemoteStartTransaction\",{\"connectorId\":1,\"idTag\":\"mIdTag\",\"chargingProfile\":{\"chargingProfileId\":9,\"stackLevel\":0,\"chargingProfilePurpose\":\"TxProfile\",\"chargingProfileKind\":\"Relative\",\"chargingSchedule\":{\"chargingRateUnit\":\"A\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":20,\"numberPhases\":1}]}}}]"

#define SCPROFILE_10_ABSOLUTE_LIMIT_5KW        "[2,\"testmsg\",\"SetChargingProfile\",{\"connectorId\":0,\"csChargingProfiles\":{\"chargingProfileId\":10,\"stackLevel\":0,\"chargingProfilePurpose\":\"ChargePointMaxProfile\",\"chargingProfileKind\":\"Absolute\",\"chargingSchedule\":{\"startSchedule\":\"2023-01-01T00:00:00.000Z\",\"chargingRateUnit\":\"W\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":5000,\"numberPhases\":3}]}}}]"


using namespace MicroOcpp;


TEST_CASE( "SmartCharging" ) {
    printf("\nRun %s\n",  "SmartCharging");

    //initialize Context with dummy socket
    LoopbackConnection loopback;
    mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));

    auto context = getOcppContext();
    auto& model = context->getModel();

    mocpp_set_timer(custom_timer_cb);

    model.getClock().setTime(BASE_TIME);

    endTransaction();

    SECTION("Load Smart Charging Service"){

        REQUIRE(!model.getSmartChargingService());

        setSmartChargingOutput([] (float, float, int) {});

        REQUIRE(model.getSmartChargingService());
    }

    setSmartChargingOutput([] (float, float, int) {});
    auto scService = model.getSmartChargingService();

    scService->clearChargingProfile([] (int, int, ChargingProfilePurposeType, int) {
        return true;
    });

    SECTION("Set Charging Profile and clear") {

        unsigned int count = 0;
        scService->clearChargingProfile([&count] (int, int, ChargingProfilePurposeType, int) {
            count++;
            return true;
        });

        REQUIRE(count == 0);

        loopback.sendTXT(SCPROFILE_0, strlen(SCPROFILE_0));

        //check if filter works by comparing the outcome of returning false and true and repeating the test
        count = 0;
        scService->clearChargingProfile([&count] (int, int, ChargingProfilePurposeType, int) {
            count++;
            return false;
        });

        REQUIRE(count == 1);

        count = 0;
        scService->clearChargingProfile([&count] (int, int, ChargingProfilePurposeType, int) {
            count++;
            return true;
        });

        REQUIRE(count == 1);

        count = 0;
        scService->clearChargingProfile([&count] (int, int, ChargingProfilePurposeType, int) {
            count++;
            return true;
        });

        REQUIRE(count == 0);
    }

    SECTION("Charging Profiles persistency over reboots") {

        loopback.sendTXT(SCPROFILE_0, strlen(SCPROFILE_0));

        mocpp_deinitialize();

        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));

        setSmartChargingOutput([] (float, float, int) {});
        scService = getOcppContext()->getModel().getSmartChargingService();

        unsigned int count = 0;
        scService->clearChargingProfile([&count] (int, int, ChargingProfilePurposeType, int) {
            count++;
            return true;
        });

        REQUIRE (count == 1);
    }

    SECTION("Set conflicting profile") {

        loopback.sendTXT(SCPROFILE_0, strlen(SCPROFILE_0));

        loopback.sendTXT(SCPROFILE_0_ALT_SAME_ID, strlen(SCPROFILE_0_ALT_SAME_ID));

        unsigned int count = 0;
        scService->clearChargingProfile([&count] (int, int, ChargingProfilePurposeType, int) {
            count++;
            return true;
        });

        REQUIRE(count == 1);

        loopback.sendTXT(SCPROFILE_0, strlen(SCPROFILE_0));

        loopback.sendTXT(SCPROFILE_0_ALT_SAME_STACKEVEL_PURPOSE, strlen(SCPROFILE_0_ALT_SAME_STACKEVEL_PURPOSE));

        count = 0;
        scService->clearChargingProfile([&count] (int, int, ChargingProfilePurposeType, int) {
            count++;
            return true;
        });

        REQUIRE(count == 1);
    }

    SECTION("Set charging profile via RmtStartTx") {
        
        float current = -1.f;
        setSmartChargingOutput([&current] (float, float limit_current, int) {
            current = limit_current;
        });

        loop();

        loopback.sendTXT(SCPROFILE_9_VIA_RMTSTARTTX_20A, strlen(SCPROFILE_9_VIA_RMTSTARTTX_20A));

        loop();

        REQUIRE((current > 19.99f && current < 20.01f));

        endTransaction();

        loop();
    }

    SECTION("Set ChargePointMaxProfile - Absolute") {
        
        float current = -1.f;
        setSmartChargingOutput([&current] (float, float limit_current, int) {
            current = limit_current;
        });

        loopback.sendTXT(SCPROFILE_1_ABSOLUTE_LIMIT_16A, strlen(SCPROFILE_1_ABSOLUTE_LIMIT_16A));

        loop();

        REQUIRE((current > 15.99f && current < 16.01f));
    }

    SECTION("Set TxDefaultProfile - Relative") {

        float current = -1.f;
        setSmartChargingOutput([&current] (float, float limit_current, int) {
            current = limit_current;
        });
        
        loopback.sendTXT(SCPROFILE_2_RELATIVE_TXDEF_24A, strlen(SCPROFILE_2_RELATIVE_TXDEF_24A));

        loop();

        REQUIRE(current < 0.f);

        beginTransaction_authorized("mIdTag");

        loop();

        REQUIRE((current > 23.99f && current < 24.01f));

        endTransaction();

        loop();

        REQUIRE(current < 0.f);
    }

    SECTION("Set TxProfile - tx fit and mismatch") {

        float current = -1.f;
        setSmartChargingOutput([&current] (float, float limit_current, int) {
            current = limit_current;
        });
        
        //send before transaction - expect rejection
        loopback.sendTXT(SCPROFILE_3_TXPROF_TXID123_20A, strlen(SCPROFILE_3_TXPROF_TXID123_20A));

        unsigned int count = 0;
        scService->clearChargingProfile([&count] (int, int, ChargingProfilePurposeType, int) {
            count++;
            return true;
        });

        REQUIRE(count == 0);

        loop();
        beginTransaction_authorized("mIdTag");

        //send during transaction but wrong txId - expect rejection
        loopback.sendTXT(SCPROFILE_3_TXPROF_TXID123_20A, strlen(SCPROFILE_3_TXPROF_TXID123_20A));

        loop();

        count = 0;
        scService->clearChargingProfile([&count] (int, int, ChargingProfilePurposeType, int) {
            count++;
            return true;
        });

        REQUIRE(count == 0);

        getTransaction()->setTransactionId(123);

        //send during tx with matchin txId - accept
        loopback.sendTXT(SCPROFILE_3_TXPROF_TXID123_20A, strlen(SCPROFILE_3_TXPROF_TXID123_20A));

        loop();

        REQUIRE((current > 19.99f && current < 20.01f));

        endTransaction();

        loop();

        //check if SCService deleted TxProfiles after tx
        count = 0;
        scService->clearChargingProfile([&count] (int, int, ChargingProfilePurposeType, int) {
            count++;
            return true;
        });

        REQUIRE(count == 0);
    }

    SECTION("Time validity check") {

        float current = -1.f;
        setSmartChargingOutput([&current] (float, float limit_current, int) {
            current = limit_current;
        });
        
        loopback.sendTXT(SCPROFILE_4_VALID_FROM_2024_16A, strlen(SCPROFILE_4_VALID_FROM_2024_16A));

        loopback.sendTXT(SCPROFILE_5_VALID_UNTIL_2022_16A, strlen(SCPROFILE_5_VALID_UNTIL_2022_16A));

        loop();

        REQUIRE(current < 0.f);

        //now reach validity period of future profile
        model.getClock().setTime("2024-01-01T00:00:00.000Z");

        loop();

        REQUIRE((current > 15.99f && current < 16.01f));
    }

    SECTION("Multiple periods") {
        
        float current = -1.f;
        setSmartChargingOutput([&current] (float, float limit_current, int) {
            current = limit_current;
        });

        loopback.sendTXT(SCPROFILE_6_MULTIPLE_PERIODS_16A_20A, strlen(SCPROFILE_6_MULTIPLE_PERIODS_16A_20A));

        loop();

        REQUIRE((current > 15.99f && current < 16.01f));

        //now reach next period
        model.getClock().setTime("2023-01-01T01:00:00.000Z");

        loop();

        REQUIRE((current > 19.99f && current < 20.01f));
    }

    SECTION("Recurring profiles - Daily") {
        
        float current = -1.f;
        setSmartChargingOutput([&current] (float, float limit_current, int) {
            current = limit_current;
        });

        loopback.sendTXT(SCPROFILE_7_RECURRING_DAY_2H_16A_20A, strlen(SCPROFILE_7_RECURRING_DAY_2H_16A_20A));

        loop();

        REQUIRE((current > 15.99f && current < 16.01f));

        //now exceed duration
        model.getClock().setTime("2023-01-01T02:00:00.000Z");

        loop();

        REQUIRE(current < 0.f);

        //check second period three days afterwards
        model.getClock().setTime("2023-01-04T01:00:00.000Z");

        loop();

        REQUIRE((current > 19.99f && current < 20.01f));
    }

    SECTION("Recurring profiles - Weekly") {
        
        float current = -1.f;
        setSmartChargingOutput([&current] (float, float limit_current, int) {
            current = limit_current;
        });

        loopback.sendTXT(SCPROFILE_8_RECURRING_WEEK_2H_10A_12A, strlen(SCPROFILE_8_RECURRING_WEEK_2H_10A_12A));

        loop();

        REQUIRE((current > 9.99f && current < 10.01f));

        //now exceed duration
        model.getClock().setTime("2023-01-01T02:00:00.000Z");

        loop();

        REQUIRE(current < 0.f);

        //check second period three weeks afterwards
        model.getClock().setTime("2023-01-22T01:00:00.000Z");

        loop();

        REQUIRE((current > 11.99f && current < 12.01f));
    }

    SECTION("Stacking recurring profiles") {

        float current = -1.f;
        setSmartChargingOutput([&current] (float, float limit_current, int) {
            current = limit_current;
        });

        loopback.sendTXT(SCPROFILE_7_RECURRING_DAY_2H_16A_20A, strlen(SCPROFILE_7_RECURRING_DAY_2H_16A_20A)); //stackLevel: 0

        loopback.sendTXT(SCPROFILE_8_RECURRING_WEEK_2H_10A_12A, strlen(SCPROFILE_8_RECURRING_WEEK_2H_10A_12A)); //stackLevel: 1

        loop();

        REQUIRE((current > 9.99f && current < 10.01f)); //Weekly schedule prevails

        //check again during the week
        model.getClock().setTime("2023-01-03T00:00:00.000Z");

        loop();

        REQUIRE((current > 15.99f && current < 16.01f)); //Weekly schedule out of duration, only Daily defined

        //check again three weeks later
        model.getClock().setTime("2023-01-22T00:00:00.000Z");

        loop();

        REQUIRE((current > 9.99f && current < 10.01f)); //Weekly schedule prevails again

        //check again during the week
        model.getClock().setTime("2023-01-23T00:00:00.000Z");

        loop();

        REQUIRE((current > 15.99f && current < 16.01f)); //Weekly schedule out of duration again, only Daily defined again
    }

    SECTION("TxProfile capped by ChargePointMaxProfile") {

        float current = -1.f;
        int numberPhases = -1;
        setSmartChargingOutput([&current, &numberPhases] (float, float limit_current, int limit_numberPhases) {
            current = limit_current;
            numberPhases = limit_numberPhases;
        });

        loop();

        loopback.sendTXT(SCPROFILE_9_VIA_RMTSTARTTX_20A, strlen(SCPROFILE_9_VIA_RMTSTARTTX_20A));

        loop();

        loopback.sendTXT(SCPROFILE_1_ABSOLUTE_LIMIT_16A, strlen(SCPROFILE_1_ABSOLUTE_LIMIT_16A));

        loop();

        REQUIRE((current > 15.99f && current < 16.01f)); //current limited by ChargePointMaxProfile
        REQUIRE(numberPhases == 1); //numberPhases limited by TxProfile

        endTransaction();

        loop();
    }

    SECTION("TxProfile and ChargePointMaxProfile with mixed units") {

        float power = -1.f;
        float current = -1.f;
        setSmartChargingOutput([&power, &current] (float limit_power, float limit_current, int) {
            power = limit_power;
            current = limit_current;
        });

        loop();

        loopback.sendTXT(SCPROFILE_9_VIA_RMTSTARTTX_20A, strlen(SCPROFILE_9_VIA_RMTSTARTTX_20A));

        loop();

        loopback.sendTXT(SCPROFILE_10_ABSOLUTE_LIMIT_5KW, strlen(SCPROFILE_10_ABSOLUTE_LIMIT_5KW));

        loop();

        REQUIRE((power > 4999.f && power < 5001.f)); //ChargePointMaxProfile defines power
        REQUIRE((current > 19.99f && current < 20.01f)); //TxProfile defines current

        endTransaction();

        loop();
    }

    SECTION("Get composite schedule") {

        loopback.sendTXT(SCPROFILE_6_MULTIPLE_PERIODS_16A_20A, strlen(SCPROFILE_6_MULTIPLE_PERIODS_16A_20A));

        bool checkProcessed = false;

        auto getCompositeSchedule = makeRequest(new Ocpp16::CustomOperation(
                "GetCompositeSchedule",
                [] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(3)));
                    auto payload = doc->to<JsonObject>();
                    payload["connectorId"] = 1;
                    payload["duration"] = 86400;
                    payload["chargingRateUnit"] = "A";
                    return doc;},
                [&checkProcessed, &model] (JsonObject payload) {
                    //receive conf
                    checkProcessed = true;

                    REQUIRE(!strcmp(payload["status"], "Accepted"));
                    REQUIRE(payload["connectorId"] == 1);
                    
                    char checkScheduleStart [JSONDATE_LENGTH + 1];
                    model.getClock().now().toJsonString(checkScheduleStart, JSONDATE_LENGTH + 1);
                    REQUIRE(!strcmp(payload["scheduleStart"], checkScheduleStart));

                    JsonObject chargingScheduleJson = payload["chargingSchedule"];
                    ChargingSchedule schedule;
                    bool success = loadChargingSchedule(chargingScheduleJson, schedule);

                    REQUIRE(success);
                    REQUIRE(schedule.chargingSchedulePeriod.size() == 2);

                    REQUIRE((schedule.chargingSchedulePeriod[0].limit > 15.99f &&
                            schedule.chargingSchedulePeriod[0].limit < 16.01f));
                    REQUIRE(schedule.chargingSchedulePeriod[0].startPeriod == 0);

                    REQUIRE((schedule.chargingSchedulePeriod[1].limit > 19.99f &&
                            schedule.chargingSchedulePeriod[1].limit < 20.01f));
                    REQUIRE(schedule.chargingSchedulePeriod[1].startPeriod == 3600);
                }
        ));

        context->initiateRequest(std::move(getCompositeSchedule));

        loop();

        REQUIRE(checkProcessed);
    }

    SECTION("Get composite schedule with definition gap") {

        loopback.sendTXT(SCPROFILE_7_RECURRING_DAY_2H_16A_20A, strlen(SCPROFILE_7_RECURRING_DAY_2H_16A_20A));

        auto schedule = scService->getCompositeSchedule(1, 86401);

        REQUIRE(schedule != nullptr);
        REQUIRE(schedule->duration == 86401);
        REQUIRE(schedule->chargingSchedulePeriod.size() == 4);
        
        REQUIRE((schedule->chargingSchedulePeriod[0].limit > 15.99f &&
                schedule->chargingSchedulePeriod[0].limit < 16.01f));
        REQUIRE(schedule->chargingSchedulePeriod[0].startPeriod == 0);

        REQUIRE((schedule->chargingSchedulePeriod[1].limit > 19.99f &&
                schedule->chargingSchedulePeriod[1].limit < 20.01f));
        REQUIRE(schedule->chargingSchedulePeriod[1].startPeriod == 3600);

        REQUIRE(schedule->chargingSchedulePeriod[2].limit < 0.f); //undefined during this period
        REQUIRE(schedule->chargingSchedulePeriod[2].startPeriod == 2 * 3600);

        REQUIRE((schedule->chargingSchedulePeriod[3].limit > 15.99f &&
                schedule->chargingSchedulePeriod[3].limit < 16.01f));
        REQUIRE(schedule->chargingSchedulePeriod[3].startPeriod == 86400);
    }

    scService->clearChargingProfile([] (int, int, ChargingProfilePurposeType, int) {
        return true;
    });

    mocpp_deinitialize();

}
