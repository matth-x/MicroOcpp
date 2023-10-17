#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>
#include "./catch2/catch.hpp"
#include "./helpers/testHelper.h"

#define BASE_TIME "2023-01-01T00:00:00.000Z"

using namespace MicroOcpp;

TEST_CASE("Metering") {
    printf("\nRun %s\n",  "Metering");

    //initialize Context with dummy socket
    LoopbackConnection loopback;
    mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));

    auto context = getOcppContext();
    auto& model = context->getModel();

    mocpp_set_timer(custom_timer_cb);

    model.getClock().setTime(BASE_TIME);

    endTransaction();

    SECTION("Configure Metering Service") {

        addMeterValueInput([] () {
            return 0;
        }, "Energy.Active.Import.Register");

        addMeterValueInput([] () {
            return 0;
        }, "Voltage");

        auto MeterValuesSampledDataString = declareConfiguration<const char*>("MeterValuesSampledData","");
        MeterValuesSampledDataString->setString("");

        bool checkProcessed = false;

        //set up measurands and check validation
        sendRequest("ChangeConfiguration",
            [] () {
                //create req
                auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(2)));
                auto payload = doc->to<JsonObject>();
                payload["key"] = "MeterValuesSampledData";
                payload["value"] = "Energy.Active.Import.Register,INVALID,Voltage"; //invalid request
                return doc;
            }, [&checkProcessed] (JsonObject payload) {
                checkProcessed = true;
                REQUIRE(!strcmp(payload["status"], "Rejected"));
            });

        loop();

        REQUIRE(checkProcessed);
        REQUIRE(!strcmp(MeterValuesSampledDataString->getString(), ""));

        checkProcessed = false;

        sendRequest("ChangeConfiguration",
            [] () {
                //create req
                auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(2)));
                auto payload = doc->to<JsonObject>();
                payload["key"] = "MeterValuesSampledData";
                payload["value"] = "Voltage,Energy.Active.Import.Register"; //valid request
                return doc;
            }, [&checkProcessed, &model] (JsonObject payload) {
                checkProcessed = true;
                REQUIRE(!strcmp(payload["status"], "Accepted"));
            });

        loop();

        REQUIRE(checkProcessed);
        REQUIRE(!strcmp(MeterValuesSampledDataString->getString(), "Voltage,Energy.Active.Import.Register"));
    }

    SECTION("Capture Periodic data") {

        Timestamp base;
        base.setTime(BASE_TIME);

        addMeterValueInput([base] () {
            //simulate 3600W consumption
            return getOcppContext()->getModel().getClock().now() - base;
        }, "Energy.Active.Import.Register");

        auto MeterValuesSampledDataString = declareConfiguration<const char*>("MeterValuesSampledData","", CONFIGURATION_FN);
        MeterValuesSampledDataString->setString("Energy.Active.Import.Register");

        auto MeterValueSampleIntervalInt = declareConfiguration<int>("MeterValueSampleInterval",0, CONFIGURATION_FN);
        MeterValueSampleIntervalInt->setInt(10);

        auto MeterValueCacheSizeInt = declareConfiguration<int>(MO_CONFIG_EXT_PREFIX "MeterValueCacheSize", 0);
        MeterValueCacheSizeInt->setInt(2);

        bool checkProcessed = false;

        setOnReceiveRequest("MeterValues", [base, &checkProcessed] (JsonObject payload) {
            checkProcessed = true;
            Timestamp t0, t1;
            t0.setTime(payload["meterValue"][0]["timestamp"] | "");
            t1.setTime(payload["meterValue"][1]["timestamp"] | "");

            REQUIRE((t0 - base >= 10 && t0 - base <= 11));
            REQUIRE((t1 - base >= 20 && t1 - base <= 21));

            REQUIRE(!strcmp(payload["meterValue"][0]["sampledValue"][0]["context"] | "", "Sample.Periodic"));
        });

        loop();

        model.getClock().setTime(BASE_TIME);

        auto trackMtime = mtime;

        beginTransaction_authorized("mIdTag");

        loop();

        mtime = trackMtime + 10 * 1000;

        loop(),

        mtime = trackMtime + 20 * 1000;

        loop();

        endTransaction();

        loop();

        REQUIRE(checkProcessed);
    }

    SECTION("Capture Clock-aligned data") {

        Timestamp base;
        base.setTime(BASE_TIME);

        addMeterValueInput([base] () {
            //simulate 3600W consumption
            return getOcppContext()->getModel().getClock().now() - base;
        }, "Energy.Active.Import.Register");

        //disablee sampled data
        auto MeterValueSampleIntervalInt = declareConfiguration<int>("MeterValueSampleInterval",0, CONFIGURATION_FN);
        MeterValueSampleIntervalInt->setInt(0);

        auto ClockAlignedDataIntervalInt  = declareConfiguration<int>("ClockAlignedDataInterval", 0, CONFIGURATION_FN);
        ClockAlignedDataIntervalInt->setInt(900);

        auto MeterValuesAlignedDataString = declareConfiguration<const char*>("MeterValuesAlignedData", "", CONFIGURATION_FN);
        MeterValuesAlignedDataString->setString("Energy.Active.Import.Register");

        auto MeterValueCacheSizeInt = declareConfiguration<int>(MO_CONFIG_EXT_PREFIX "MeterValueCacheSize", 0, CONFIGURATION_FN);
        MeterValueCacheSizeInt->setInt(2);

        bool checkProcessed = false;

        setOnReceiveRequest("MeterValues", [base, &checkProcessed] (JsonObject payload) {
            checkProcessed = true;
            Timestamp t0, t1;
            t0.setTime(payload["meterValue"][0]["timestamp"] | "");
            t1.setTime(payload["meterValue"][1]["timestamp"] | "");

            REQUIRE((t0 - base >= 900 && t0 - base <= 901));
            REQUIRE((t1 - base >= 1800 && t1 - base <= 1801));

            REQUIRE(!strcmp(payload["meterValue"][0]["sampledValue"][0]["context"] | "", "Sample.Clock"));
        });

        model.getClock().setTime("2023-01-01T00:00:10Z");
        loop();

        beginTransaction_authorized("mIdTag");

        loop();

        model.getClock().setTime("2023-01-01T00:10:00Z");
        loop();

        model.getClock().setTime("2023-01-01T00:15:00Z");
        loop();

        model.getClock().setTime("2023-01-01T00:29:50Z");
        loop();

        model.getClock().setTime("2023-01-01T00:30:00Z");
        loop();

        endTransaction();

        loop();

        REQUIRE(checkProcessed);
    }

    SECTION("Capture transaction-aligned data") {

        Timestamp base;
        base.setTime(BASE_TIME);

        addMeterValueInput([base] () {
            //simulate 3600W consumption
            return getOcppContext()->getModel().getClock().now() - base;
        }, "Energy.Active.Import.Register");

        auto MeterValueSampleIntervalInt = declareConfiguration<int>("MeterValueSampleInterval",0, CONFIGURATION_FN);
        MeterValueSampleIntervalInt->setInt(10);

        auto StopTxnSampledDataString = declareConfiguration<const char*>("StopTxnSampledData", "", CONFIGURATION_FN);
        StopTxnSampledDataString->setString("Energy.Active.Import.Register");

        loop();

        model.getClock().setTime(BASE_TIME);

        beginTransaction_authorized("mIdTag");

        loop();

        mocpp_deinitialize(); //check if StopData is stored over reboots

        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));

        addMeterValueInput([base] () {
            //simulate 3600W consumption
            return getOcppContext()->getModel().getClock().now() - base;
        }, "Energy.Active.Import.Register");

        bool checkProcessed = false;

        setOnReceiveRequest("StopTransaction", [base, &checkProcessed] (JsonObject payload) {
            checkProcessed = true;
            Timestamp t0, t1;
            t0.setTime(payload["transactionData"][0]["timestamp"] | "");
            t1.setTime(payload["transactionData"][1]["timestamp"] | "");

            REQUIRE((t0 - base >= 0 && t0 - base <= 1));
            REQUIRE((t1 - base >= 3600 && t1 - base <= 3601));

            REQUIRE(!strcmp(payload["transactionData"][0]["sampledValue"][0]["context"] | "", "Transaction.Begin"));
            REQUIRE(!strcmp(payload["transactionData"][1]["sampledValue"][0]["context"] | "", "Transaction.End"));
        });

        loop();

        getOcppContext()->getModel().getClock().setTime("2023-01-01T01:00:00Z");

        endTransaction();

        loop();

        REQUIRE(checkProcessed);
    }

    SECTION("Capture measurements at connectorId 0") {

        Timestamp base;
        base.setTime(BASE_TIME);

        const unsigned int connectorId = 0;

        addMeterValueInput([base] () {
                //simulate 3600W consumption
                return 3600;
            }, 
            "Power.Active.Import",
            nullptr,
            nullptr,
            nullptr,
            connectorId);

        auto MeterValuesSampledDataString = declareConfiguration<const char*>("MeterValuesSampledData","", CONFIGURATION_FN);
        MeterValuesSampledDataString->setString("Power.Active.Import");

        auto MeterValueSampleIntervalInt = declareConfiguration<int>("MeterValueSampleInterval",0, CONFIGURATION_FN);
        MeterValueSampleIntervalInt->setInt(10);

        bool checkProcessed = false;

        setOnReceiveRequest("MeterValues", [base, &checkProcessed] (JsonObject payload) {
            checkProcessed = true;
            REQUIRE((!strcmp(payload["meterValue"][0]["sampledValue"][0]["value"] | "", "3600") ||
                     !strcmp(payload["meterValue"][0]["sampledValue"][0]["value"] | "", "3600.0")));
        });

        loop();

        mtime += 10 * 1000;

        loop();

        REQUIRE(checkProcessed);
    }

    SECTION("Change measurands live") {

        Timestamp base;
        base.setTime(BASE_TIME);

        addMeterValueInput([base] () {
            //simulate 3600W consumption
            return getOcppContext()->getModel().getClock().now() - base;
        }, "Energy.Active.Import.Register");

        addMeterValueInput([] () {
            return 3600;
        }, "Power.Active.Import");

        auto MeterValuesSampledDataString = declareConfiguration<const char*>("MeterValuesSampledData","", CONFIGURATION_FN);
        MeterValuesSampledDataString->setString("Energy.Active.Import.Register");

        auto MeterValueSampleIntervalInt = declareConfiguration<int>("MeterValueSampleInterval",0, CONFIGURATION_FN);
        MeterValueSampleIntervalInt->setInt(10);

        auto MeterValueCacheSizeInt = declareConfiguration<int>(MO_CONFIG_EXT_PREFIX "MeterValueCacheSize", 0, CONFIGURATION_FN);
        MeterValueCacheSizeInt->setInt(10);

        bool checkProcessed = false;

        setOnReceiveRequest("MeterValues", [base, &checkProcessed] (JsonObject payload) {
            checkProcessed = true;
            Timestamp t0, t1;
            t0.setTime(payload["meterValue"][0]["timestamp"] | "");
            t1.setTime(payload["meterValue"][1]["timestamp"] | "");

            REQUIRE((t0 - base >= 10 && t0 - base <= 11));
            REQUIRE((t1 - base >= 20 && t1 - base <= 21));
            
            REQUIRE(!strcmp(payload["meterValue"][0]["sampledValue"][0]["measurand"] | "", "Energy.Active.Import.Register"));
            REQUIRE(!strcmp(payload["meterValue"][1]["sampledValue"][0]["measurand"] | "", "Power.Active.Import"));
        });

        loop();

        model.getClock().setTime(BASE_TIME);

        auto trackMtime = mtime;

        beginTransaction_authorized("mIdTag");

        loop();

        mtime = trackMtime + 10 * 1000;

        loop();

        MeterValuesSampledDataString->setString("Power.Active.Import");

        mtime = trackMtime + 20 * 1000;

        loop();

        endTransaction();

        loop();

        REQUIRE(checkProcessed);
    }

    mocpp_deinitialize();
}
