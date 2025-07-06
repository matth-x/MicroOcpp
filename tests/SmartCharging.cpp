// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/SmartCharging/SmartChargingService.h>
#include <MicroOcpp/Debug.h>
#include <catch2/catch.hpp>
#include "./helpers/testHelper.h"

#define BASE_TIME "2023-01-01T00:00:00.000Z"

#define SCPROFILE_0                            "[2,\"testmsg\",\"SetChargingProfile\",{\"connectorId\":1,\"csChargingProfiles\":{\"chargingProfileId\":0,\"stackLevel\":0,\"chargingProfilePurpose\":\"TxDefaultProfile\",\"chargingProfileKind\":\"Recurring\",\"recurrencyKind\":\"Daily\",\"validFrom\":\"2022-06-12T00:00:00.000Z\",\"validTo\":\"2023-06-21T00:00:00.000Z\",\"chargingSchedule\":{         \"duration\":1000000,\"startSchedule\":\"2023-06-18T00:00:00.000Z\",\"chargingRateUnit\":\"W\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":16,\"numberPhases\":3},{\"startPeriod\":18000,\"limit\":32,\"numberPhases\":3}],\"minChargingRate\":6}}}]"
#define SCPROFILE_0_V201                       "[2,\"testmsg\",\"SetChargingProfile\",{\"evseId\":     1,\"chargingProfile\":   {\"id\":               0,\"stackLevel\":0,\"chargingProfilePurpose\":\"TxDefaultProfile\",\"chargingProfileKind\":\"Recurring\",\"recurrencyKind\":\"Daily\",\"validFrom\":\"2022-06-12T00:00:00.000Z\",\"validTo\":\"2023-06-21T00:00:00.000Z\",\"chargingSchedule\":{\"id\":0,\"duration\":1000000,\"startSchedule\":\"2023-06-18T00:00:00.000Z\",\"chargingRateUnit\":\"W\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":16,\"numberPhases\":3},{\"startPeriod\":18000,\"limit\":32,\"numberPhases\":3}],\"minChargingRate\":6}}}]"
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

#define SCPROFILE_9_VIA_RMTSTARTTX_20A         "[2,\"testmsg\",\"RemoteStartTransaction\", {\"connectorId\":1,\"idTag\":               \"mIdTag\",                       \"chargingProfile\":{\"chargingProfileId\":9,\"stackLevel\":0,\"chargingProfilePurpose\":\"TxProfile\",\"chargingProfileKind\":\"Relative\",\"chargingSchedule\":{         \"chargingRateUnit\":\"A\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":20,\"numberPhases\":1}]}}}]"
#define SCPROFILE_9_VIA_REQSTARTTX_20A         "[2,\"testmsg\",\"RequestStartTransaction\",{\"evseId\":1,     \"idToken\":{\"idToken\":\"mIdTag\",\"type\":\"ISO14443\"},\"chargingProfile\":{\"id\":9,               \"stackLevel\":0,\"chargingProfilePurpose\":\"TxProfile\",\"chargingProfileKind\":\"Relative\",\"chargingSchedule\":{\"id\":9,\"chargingRateUnit\":\"A\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":20,\"numberPhases\":1}]}}}]"

#define SCPROFILE_10_ABSOLUTE_LIMIT_5KW        "[2,\"testmsg\",\"SetChargingProfile\",{\"connectorId\":0,\"csChargingProfiles\":{\"chargingProfileId\":10,\"stackLevel\":0,\"chargingProfilePurpose\":\"ChargePointMaxProfile\",\"chargingProfileKind\":\"Absolute\",\"chargingSchedule\":{\"startSchedule\":\"2023-01-01T00:00:00.000Z\",\"chargingRateUnit\":\"W\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":5000,\"numberPhases\":3}]}}}]"

char setChargingProfile [1024];
void updateSetChargingProfile(const char *scprofile_raw, int ocppVersion) {
    if (ocppVersion == MO_OCPP_V16) {
        auto ret = snprintf(setChargingProfile, sizeof(setChargingProfile), "%s", scprofile_raw);
        if (ret < 0 || (size_t)ret >= sizeof(setChargingProfile)) {
            FAIL("snprintf");
        }
        return;
    }

    DynamicJsonDocument doc (4096);
    auto err = deserializeJson(doc, scprofile_raw);
    if (err) {
        char buf [100];
        snprintf(buf, sizeof(buf), "JSON deserialization error: %s", err.c_str());
        FAIL(buf);
    }

    JsonObject payload = doc[3];

    payload["evseId"] = payload["connectorId"];
    payload.remove("connectorId");
    payload["chargingProfile"] = payload["csChargingProfiles"];
    payload.remove("csChargingProfiles");
    JsonObject chargingProfile = payload["chargingProfile"];
    chargingProfile["id"] = chargingProfile["chargingProfileId"];
    chargingProfile.remove("chargingProfileId");
    chargingProfile["chargingSchedule"]["id"] = chargingProfile["id"];

    auto ret = serializeJson(doc, setChargingProfile);

    if (doc.isNull() || doc.overflowed() || ret >= sizeof(setChargingProfile) - 1 || ret < 2) {
        FAIL("failure to convert JSON to OCPP 2.0.1");
    }
}

float g_limit_current = -1.f;
void setCurrentCb(float limit_current) {
    g_limit_current = limit_current;
}

using namespace MicroOcpp;


TEST_CASE( "SmartCharging" ) {
    printf("\nRun %s\n",  "SmartCharging");

    g_limit_current = -1.f;

    //initialize Context without any configs
    mo_initialize();

    mo_getContext()->setTicksCb(custom_timer_cb);

    LoopbackConnection loopback;
    mo_getContext()->setConnection(&loopback);

    auto ocppVersion = GENERATE(MO_OCPP_V16, MO_OCPP_V201);
    mo_setOcppVersion(ocppVersion);

    mo_setBootNotificationData("TestModel", "TestVendor");

    mo_setSmartChargingPowerOutput([] (float) {});

    mo_setup();

    loop();

    MicroOcpp::SmartChargingService *scService = nullptr;
    if (ocppVersion == MO_OCPP_V16) {
        scService = mo_getContext()->getModel16().getSmartChargingService();
    } else if (ocppVersion == MO_OCPP_V201) {
        scService = mo_getContext()->getModel201().getSmartChargingService();
    }
    REQUIRE(scService != nullptr);

    SECTION("Clean up files") {
        auto filesystem = mo_getFilesystem();
        FilesystemUtils::removeByPrefix(filesystem, "");
    }

    SECTION("Set Charging Profile and clear") {

        REQUIRE(scService->getChargingProfilesCount() == 0);

        if (ocppVersion == MO_OCPP_V16) {
            loopback.sendTXT(SCPROFILE_0, strlen(SCPROFILE_0));
        } else if (ocppVersion == MO_OCPP_V201) {
            loopback.sendTXT(SCPROFILE_0_V201, strlen(SCPROFILE_0_V201));
        }
        updateSetChargingProfile(SCPROFILE_0, ocppVersion);
        loopback.sendTXT(setChargingProfile, strlen(setChargingProfile));

        loop();

        REQUIRE(scService->getChargingProfilesCount() == 1);

        REQUIRE(scService->clearChargingProfile(-1, -1, MicroOcpp::ChargingProfilePurposeType::UNDEFINED, -1));

        REQUIRE(scService->getChargingProfilesCount() == 0);
    }

    SECTION("Charging Profiles persistency over reboots") {

        updateSetChargingProfile(SCPROFILE_0, ocppVersion);
        loopback.sendTXT(setChargingProfile, strlen(setChargingProfile));
        loop();

        mo_deinitialize();

        mo_initialize();
        mo_getContext()->setConnection(&loopback);
        mo_setOcppVersion(ocppVersion);
        mo_setup();
        if (ocppVersion == MO_OCPP_V16) {
            scService = mo_getContext()->getModel16().getSmartChargingService();
        } else if (ocppVersion == MO_OCPP_V201) {
            scService = mo_getContext()->getModel201().getSmartChargingService();
        }

        REQUIRE(scService->getChargingProfilesCount() == 1);
        REQUIRE(scService->clearChargingProfile(-1, -1, MicroOcpp::ChargingProfilePurposeType::UNDEFINED, -1));
    }

    SECTION("Set conflicting profile") {

        updateSetChargingProfile(SCPROFILE_0, ocppVersion);
        loopback.sendTXT(setChargingProfile, strlen(setChargingProfile));
        loop();

        updateSetChargingProfile(SCPROFILE_0_ALT_SAME_ID, ocppVersion);
        loopback.sendTXT(setChargingProfile, strlen(setChargingProfile));
        loop();

        REQUIRE(scService->getChargingProfilesCount() == 1);
        REQUIRE(scService->clearChargingProfile(-1, -1, MicroOcpp::ChargingProfilePurposeType::UNDEFINED, -1));

        updateSetChargingProfile(SCPROFILE_0, ocppVersion);
        loopback.sendTXT(setChargingProfile, strlen(setChargingProfile));
        loop();

        updateSetChargingProfile(SCPROFILE_0_ALT_SAME_STACKEVEL_PURPOSE, ocppVersion);
        loopback.sendTXT(setChargingProfile, strlen(setChargingProfile));
        loop();

        REQUIRE(scService->getChargingProfilesCount() == 1);
        REQUIRE(scService->clearChargingProfile(-1, -1, MicroOcpp::ChargingProfilePurposeType::UNDEFINED, -1));
    }

    SECTION("Set charging profile via RmtStartTx") {
        
        mo_setSmartChargingCurrentOutput(setCurrentCb);
        g_limit_current = -1.f;

        loop();

        if (ocppVersion == MO_OCPP_V16) {
            loopback.sendTXT(SCPROFILE_9_VIA_RMTSTARTTX_20A, strlen(SCPROFILE_9_VIA_RMTSTARTTX_20A));
        } else if (ocppVersion == MO_OCPP_V201) {
            loopback.sendTXT(SCPROFILE_9_VIA_REQSTARTTX_20A, strlen(SCPROFILE_9_VIA_REQSTARTTX_20A));
        }

        loop();

        REQUIRE(g_limit_current > 19.99f);
        REQUIRE(g_limit_current < 20.01f);

        mo_endTransaction(nullptr, nullptr);

        loop();
    }

    #if 0

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
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(3));
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
                    
                    char checkScheduleStart [MO_JSONDATE_SIZE];
                    model.getClock().now().toJsonString(checkScheduleStart, MO_JSONDATE_SIZE);
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

    SECTION("SmartCharging memory limits - MaxChargingProfilesInstalled") {

        loop();

        bool checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "SetChargingProfile",
                [] () {
                    //create req
                    StaticJsonDocument<2048> raw;
                    deserializeJson(raw, SCPROFILE_2_RELATIVE_TXDEF_24A);
                    auto doc = makeJsonDoc("UnitTests", 2048);
                    *doc = raw[3];
                    return doc;},
                [&checkProcessed] (JsonObject response) {
                    checkProcessed = true;
                    REQUIRE( !strcmp(response["status"] | "_Undefined", "Accepted") );
                }
        )));
        loop();
        REQUIRE( checkProcessed );

        checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "SetChargingProfile",
                [] () {
                    //create req
                    StaticJsonDocument<2048> raw;
                    deserializeJson(raw, SCPROFILE_0_ALT_SAME_ID);
                    auto doc = makeJsonDoc("UnitTests", 2048);
                    *doc = raw[3];
                    (*doc)["connectorId"] = 2;
                    return doc;},
                [&checkProcessed] (JsonObject response) {
                    checkProcessed = true;
                    REQUIRE( !strcmp(response["status"] | "_Undefined", "Accepted") );
                }
        )));
        loop();
        REQUIRE( checkProcessed );

        checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "SetChargingProfile",
                [] () {
                    //create req
                    StaticJsonDocument<2048> raw;
                    deserializeJson(raw, SCPROFILE_1_ABSOLUTE_LIMIT_16A);
                    auto doc = makeJsonDoc("UnitTests", 2048);
                    *doc = raw[3];
                    return doc;},
                [&checkProcessed] (JsonObject response) {
                    checkProcessed = true;
                    REQUIRE( !strcmp(response["status"] | "_Undefined", "Accepted") );
                }
        )));
        loop();
        REQUIRE( checkProcessed );

        // 3 distinct ChargingProfiles installed. Check if further Profiles are rejected correctly

        for (size_t i = 0; i < 2; i++) {
            // replace existing profile - OK

            checkProcessed = false;
            getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                    "SetChargingProfile",
                    [] () {
                        //create req
                        StaticJsonDocument<2048> raw;
                        deserializeJson(raw, SCPROFILE_1_ABSOLUTE_LIMIT_16A);
                        auto doc = makeJsonDoc("UnitTests", 2048);
                        *doc = raw[3];
                        return doc;},
                    [&checkProcessed] (JsonObject response) {
                        checkProcessed = true;
                        REQUIRE( !strcmp(response["status"] | "_Undefined", "Accepted") );
                    }
            )));
            loop();
            REQUIRE( checkProcessed );
        }

        for (size_t i = 0; i < 2; i++) {
            // try to install additional profile - not okay

            checkProcessed = false;
            getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                    "SetChargingProfile",
                    [] () {
                        //create req
                        StaticJsonDocument<2048> raw;
                        deserializeJson(raw, SCPROFILE_5_VALID_UNTIL_2022_16A);
                        auto doc = makeJsonDoc("UnitTests", 2048);
                        *doc = raw[3];
                        return doc;},
                    [&checkProcessed] (JsonObject response) {
                        checkProcessed = true;
                        REQUIRE( !strcmp(response["status"] | "_Undefined", "Rejected") );
                    }
            )));
            loop();
            REQUIRE( checkProcessed );
        }
    }

    SECTION("SmartCharging memory limits - ChargeProfileMaxStackLevel") {

        loop();

        bool checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "SetChargingProfile",
                [] () {
                    //create req
                    StaticJsonDocument<2048> raw;
                    deserializeJson(raw, SCPROFILE_2_RELATIVE_TXDEF_24A);
                    auto doc = makeJsonDoc("UnitTests", 2048);
                    *doc = raw[3];
                    (*doc)["csChargingProfiles"]["stackLevel"] = MO_ChargeProfileMaxStackLevel;
                    return doc;},
                [&checkProcessed] (JsonObject response) {
                    checkProcessed = true;
                    REQUIRE( !strcmp(response["status"] | "_Undefined", "Accepted") );
                }
        )));
        loop();
        REQUIRE( checkProcessed );

        checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "SetChargingProfile",
                [] () {
                    //create req
                    StaticJsonDocument<2048> raw;
                    deserializeJson(raw, SCPROFILE_2_RELATIVE_TXDEF_24A);
                    auto doc = makeJsonDoc("UnitTests", 2048);
                    *doc = raw[3];
                    (*doc)["csChargingProfiles"]["stackLevel"] = MO_ChargeProfileMaxStackLevel + 1;
                    return doc;},
                [] (JsonObject) { }, //ignore conf
                [&checkProcessed] (const char*, const char*, JsonObject) {
                    // process error
                    checkProcessed = true;
                    return true;
                }
        )));
        loop();
        REQUIRE( checkProcessed );
    }

    SECTION("SmartCharging memory limits - ChargingScheduleMaxPeriods") {

        loop();

        bool checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "SetChargingProfile",
                [] () {
                    //create req
                    StaticJsonDocument<2048> raw;
                    deserializeJson(raw, SCPROFILE_2_RELATIVE_TXDEF_24A);
                    auto doc = makeJsonDoc("UnitTests", 2048);
                    *doc = raw[3];
                    JsonArray chargingSchedulePeriod = (*doc)["csChargingProfiles"]["chargingSchedule"]["chargingSchedulePeriod"];
                    chargingSchedulePeriod.clear();
                    for (size_t i = 0; i < MO_ChargingScheduleMaxPeriods; i++) {
                        auto period = chargingSchedulePeriod.createNestedObject();
                        period["startPeriod"] = i;
                        period["limit"] = 16;
                    }
                    return doc;},
                [&checkProcessed] (JsonObject response) {
                    checkProcessed = true;
                    REQUIRE( !strcmp(response["status"] | "_Undefined", "Accepted") );
                }
        )));
        loop();
        REQUIRE( checkProcessed );

        checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "SetChargingProfile",
                [] () {
                    //create req
                    StaticJsonDocument<2048> raw;
                    deserializeJson(raw, SCPROFILE_2_RELATIVE_TXDEF_24A);
                    auto doc = makeJsonDoc("UnitTests", 2048);
                    *doc = raw[3];
                    JsonArray chargingSchedulePeriod = (*doc)["csChargingProfiles"]["chargingSchedule"]["chargingSchedulePeriod"];
                    chargingSchedulePeriod.clear();
                    for (size_t i = 0; i < MO_ChargingScheduleMaxPeriods + 1; i++) {
                        auto period = chargingSchedulePeriod.createNestedObject();
                        period["startPeriod"] = i;
                        period["limit"] = 16;
                    }
                    return doc;},
                [] (JsonObject) { }, //ignore conf
                [&checkProcessed] (const char*, const char*, JsonObject) {
                    // process error
                    checkProcessed = true;
                    return true;
                }
        )));
        loop();
        REQUIRE( checkProcessed );
    }

    SECTION("ChargingScheduleAllowedChargingRateUnit") {
        
        setSmartChargingOutput(nullptr);
        loop();

        // accept power, reject current
        setSmartChargingPowerOutput([] (float) { });

        bool checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "SetChargingProfile",
                [] () {
                    //create req
                    StaticJsonDocument<2048> raw;
                    deserializeJson(raw, SCPROFILE_0);
                    auto doc = makeJsonDoc("UnitTests", 2048);
                    *doc = raw[3];
                    return doc;},
                [&checkProcessed] (JsonObject response) {
                    checkProcessed = true;
                    REQUIRE( !strcmp(response["status"] | "_Undefined", "Accepted") );
                }
        )));
        loop();
        REQUIRE( checkProcessed );

        checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "SetChargingProfile",
                [] () {
                    //create req
                    StaticJsonDocument<2048> raw;
                    deserializeJson(raw, SCPROFILE_1_ABSOLUTE_LIMIT_16A);
                    auto doc = makeJsonDoc("UnitTests", 2048);
                    *doc = raw[3];
                    return doc;},
                [&checkProcessed] (JsonObject response) {
                    checkProcessed = true;
                    REQUIRE( !strcmp(response["status"] | "_Undefined", "Rejected") );
                }
        )));
        loop();
        REQUIRE( checkProcessed );

        //  reject power, accept current
        setSmartChargingPowerOutput(nullptr);
        setSmartChargingCurrentOutput([] (float) { });

        checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "SetChargingProfile",
                [] () {
                    //create req
                    StaticJsonDocument<2048> raw;
                    deserializeJson(raw, SCPROFILE_0);
                    auto doc = makeJsonDoc("UnitTests", 2048);
                    *doc = raw[3];
                    return doc;},
                [&checkProcessed] (JsonObject response) {
                    checkProcessed = true;
                    REQUIRE( !strcmp(response["status"] | "_Undefined", "Rejected") );
                }
        )));
        loop();
        REQUIRE( checkProcessed );

        checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "SetChargingProfile",
                [] () {
                    //create req
                    StaticJsonDocument<2048> raw;
                    deserializeJson(raw, SCPROFILE_1_ABSOLUTE_LIMIT_16A);
                    auto doc = makeJsonDoc("UnitTests", 2048);
                    *doc = raw[3];
                    return doc;},
                [&checkProcessed] (JsonObject response) {
                    checkProcessed = true;
                    REQUIRE( !strcmp(response["status"] | "_Undefined", "Accepted") );
                }
        )));
        loop();
        REQUIRE( checkProcessed );
    }

    #endif

    scService->clearChargingProfile(-1, -1, MicroOcpp::ChargingProfilePurposeType::UNDEFINED, -1);

    mo_deinitialize();

}
