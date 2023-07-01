#include <ArduinoOcpp.h>
#include <ArduinoOcpp/Core/Connection.h>
#include <ArduinoOcpp/Context.h>
#include <ArduinoOcpp/Model/Model.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Core/SimpleRequestFactory.h>
#include "./catch2/catch.hpp"
#include "./helpers/testHelper.h"

#define BASE_TIME "2023-01-01T00:00:00.000Z"

using namespace ArduinoOcpp;

TEST_CASE("Metering") {

    //initialize Context with dummy socket
    LoopbackConnection loopback;
    OCPP_initialize(loopback, ChargerCredentials("test-runner1234"));

    auto context = getOcppContext();
    auto& model = context->getModel();

    ao_set_timer(custom_timer_cb);

    model.getClock().setTime(BASE_TIME);

    endTransaction();

    SECTION("Configure Metering Service") {

        addMeterValueInput([] () {
            return 0;
        }, "Energy.Active.Import.Register");

        addMeterValueInput([] () {
            return 0;
        }, "Voltage");

        auto MeterValuesSampledData = declareConfiguration<const char*>("MeterValuesSampledData","", CONFIGURATION_FN);
        *MeterValuesSampledData = "";

        bool checkProcessed = false;

        //set up measurands and check validation
        sendCustomRequest("ChangeConfiguration",
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
        REQUIRE(!strcmp((const char*) *MeterValuesSampledData, ""));

        checkProcessed = false;

        sendCustomRequest("ChangeConfiguration",
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
        REQUIRE(!strcmp((const char*) *MeterValuesSampledData, "Voltage,Energy.Active.Import.Register"));
    }

    SECTION("Capture Periodic data") {

        Timestamp base;
        base.setTime(BASE_TIME);

        addMeterValueInput([base] () {
            //simulate 3600W consumption
            return getOcppContext()->getModel().getClock().now() - base;
        }, "Energy.Active.Import.Register");

        auto MeterValuesSampledData = declareConfiguration<const char*>("MeterValuesSampledData","", CONFIGURATION_FN);
        *MeterValuesSampledData = "Energy.Active.Import.Register";

        auto MeterValueSampleInterval = declareConfiguration<int>("MeterValueSampleInterval",0, CONFIGURATION_FN);
        *MeterValueSampleInterval = 10;

        auto MeterValueCacheSize = declareConfiguration("AO_MeterValueCacheSize", 0, CONFIGURATION_FN);
        *MeterValueCacheSize = 2;

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
        auto MeterValueSampleInterval = declareConfiguration<int>("MeterValueSampleInterval",0, CONFIGURATION_FN);
        *MeterValueSampleInterval = 0;

        auto ClockAlignedDataInterval  = declareConfiguration("ClockAlignedDataInterval", 0, CONFIGURATION_FN);
        *ClockAlignedDataInterval = 900;

        auto MeterValuesAlignedData = declareConfiguration<const char*>("MeterValuesAlignedData", "", CONFIGURATION_FN);
        *MeterValuesAlignedData = "Energy.Active.Import.Register";

        auto MeterValueCacheSize = declareConfiguration("AO_MeterValueCacheSize", 0, CONFIGURATION_FN);
        *MeterValueCacheSize = 2;

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

        auto MeterValueSampleInterval = declareConfiguration<int>("MeterValueSampleInterval",0, CONFIGURATION_FN);
        *MeterValueSampleInterval = 10;

        auto StopTxnSampledData = declareConfiguration<const char*>("StopTxnSampledData", "", CONFIGURATION_FN);
        *StopTxnSampledData = "Energy.Active.Import.Register";

        loop();

        model.getClock().setTime(BASE_TIME);

        beginTransaction_authorized("mIdTag");

        loop();

        OCPP_deinitialize(); //check if StopData is stored over reboots

        OCPP_initialize(loopback, ChargerCredentials("test-runner1234"));

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

        model.getClock().setTime("2023-01-01T01:00:00Z");

        endTransaction();

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

        auto MeterValuesSampledData = declareConfiguration<const char*>("MeterValuesSampledData","", CONFIGURATION_FN);
        *MeterValuesSampledData = "Energy.Active.Import.Register";

        auto MeterValueSampleInterval = declareConfiguration<int>("MeterValueSampleInterval",0, CONFIGURATION_FN);
        *MeterValueSampleInterval = 10;

        auto MeterValueCacheSize = declareConfiguration("AO_MeterValueCacheSize", 0, CONFIGURATION_FN);
        *MeterValueCacheSize = 10;

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

        *MeterValuesSampledData = "Power.Active.Import";

        mtime = trackMtime + 20 * 1000;

        loop();

        endTransaction();

        loop();

        REQUIRE(checkProcessed);
    }

    OCPP_deinitialize();
}
