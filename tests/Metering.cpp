// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Operations/CustomOperation.h>
#include <catch2/catch.hpp>
#include "./helpers/testHelper.h"

#define BASE_TIME "2023-01-01T00:00:00.000Z"

#define TRIGGER_METERVALUES "[2,\"msgId01\",\"TriggerMessage\",{\"requestedMessage\":\"MeterValues\"}]"

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
                auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(2));
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
                auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(2));
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

        bool checkProcessed = false;

        setOnReceiveRequest("MeterValues", [base, &checkProcessed] (JsonObject payload) {
            checkProcessed = true;
            Timestamp t0;
            t0.setTime(payload["meterValue"][0]["timestamp"] | "");

            REQUIRE((t0 - base >= 10 && t0 - base <= 11));

            REQUIRE(!strcmp(payload["meterValue"][0]["sampledValue"][0]["context"] | "", "Sample.Periodic"));
        });

        loop();

        model.getClock().setTime(BASE_TIME);

        auto trackMtime = mtime;

        beginTransaction_authorized("mIdTag");

        loop();

        mtime = trackMtime + 10 * 1000;

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

        bool checkProcessed = false;

        setOnReceiveRequest("MeterValues", [base, &checkProcessed] (JsonObject payload) {
            checkProcessed = true;
            Timestamp t0;
            t0.setTime(payload["meterValue"][0]["timestamp"] | "");

            REQUIRE((t0 - base >= 900 && t0 - base <= 901));

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

        configuration_save();

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

            REQUIRE(payload["transactionData"].size() >= 2);

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
            REQUIRE( !strncmp(payload["meterValue"][0]["sampledValue"][0]["value"] | "", "3600", strlen("3600")) );
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

        bool checkProcessed = false;

        setOnReceiveRequest("MeterValues", [base, &checkProcessed] (JsonObject payload) {
            checkProcessed = true;
            Timestamp t0;
            t0.setTime(payload["meterValue"][0]["timestamp"] | "");

            REQUIRE((t0 - base >= 10 && t0 - base <= 11));
            
            REQUIRE(!strcmp(payload["meterValue"][0]["sampledValue"][0]["measurand"] | "", "Power.Active.Import"));
        });

        loop();

        model.getClock().setTime(BASE_TIME);

        auto trackMtime = mtime;

        beginTransaction_authorized("mIdTag");

        MeterValuesSampledDataString->setString("Power.Active.Import");

        loop();

        mtime = trackMtime + 10 * 1000;

        loop();

        endTransaction();

        loop();

        REQUIRE(checkProcessed);
    }

    SECTION("Preserve order of tx-related msgs") {

        loopback.setConnected(false);

        declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "PreBootTransactions", true)->setBool(true);

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

        configuration_save();

        unsigned int countProcessed = 0;

        setOnReceiveRequest("StartTransaction", [&countProcessed] (JsonObject) {
            REQUIRE(countProcessed == 0);
            countProcessed++;
        });

        int assignedTxId = -1;

        setOnSendConf("StartTransaction", [&assignedTxId] (JsonObject conf) {
            assignedTxId = conf["transactionId"];
        });

        setOnReceiveRequest("MeterValues", [&countProcessed, &assignedTxId] (JsonObject req) {
            REQUIRE(countProcessed == 1);
            countProcessed++;

            int transactionId = req["transactionId"] | -1000;

            REQUIRE(assignedTxId == transactionId);
        });

        setOnReceiveRequest("StopTransaction", [&countProcessed] (JsonObject) {
            REQUIRE(countProcessed == 2);
            countProcessed++;
        });

        loop();

        auto trackMtime = mtime;

        beginTransaction_authorized("mIdTag");

        loop();

        mtime = trackMtime + 10 * 1000;

        loop();

        endTransaction();

        loop();

        loopback.setConnected(true);

        loop();

        REQUIRE(countProcessed == 3);

        /*
         * Combine test case with power loss. Start tx before power loss, then enqueue 1 MV, then StopTx
         */

        countProcessed = 0;

        beginTransaction("mIdTag");

        loop();

        mocpp_deinitialize();

        loopback.setConnected(false);

        mocpp_initialize(loopback, ChargerCredentials());
        getOcppContext()->getModel().getClock().setTime(BASE_TIME);

        base.setTime(BASE_TIME);

        addMeterValueInput([base] () {
            //simulate 3600W consumption
            return getOcppContext()->getModel().getClock().now() - base;
        }, "Energy.Active.Import.Register");

        setOnReceiveRequest("MeterValues", [&countProcessed, &assignedTxId] (JsonObject req) {
            REQUIRE(countProcessed == 1);
            countProcessed++;

            int transactionId = req["transactionId"] | -1000;

            REQUIRE(assignedTxId == transactionId);
        });

        setOnReceiveRequest("StopTransaction", [&countProcessed] (JsonObject) {
            REQUIRE(countProcessed == 2);
            countProcessed++;
        });

        trackMtime = mtime;

        loop();

        mtime = trackMtime + 10 * 1000;

        loop();

        endTransaction();

        loop();

        loopback.setConnected(true);

        loop();

        REQUIRE(countProcessed == 3);
    }

    SECTION("Queue multiple MeterValues") {

        Timestamp base;
        base.setTime(BASE_TIME);
        model.getClock().setTime(BASE_TIME);

        addMeterValueInput([base] () {
            //simulate 3600W consumption
            return getOcppContext()->getModel().getClock().now() - base;
        }, "Energy.Active.Import.Register");

        auto MeterValuesSampledDataString = declareConfiguration<const char*>("MeterValuesSampledData","", CONFIGURATION_FN);
        MeterValuesSampledDataString->setString("Energy.Active.Import.Register");

        auto MeterValueSampleIntervalInt = declareConfiguration<int>("MeterValueSampleInterval",0, CONFIGURATION_FN);
        MeterValueSampleIntervalInt->setInt(10);

        unsigned int nrInitiated = 0;
        unsigned int countProcessed = 0;

        setOnReceiveRequest("MeterValues", [&base, &nrInitiated, &countProcessed] (JsonObject payload) {
            countProcessed++;

            Timestamp t0;
            t0.setTime(payload["meterValue"][0]["timestamp"] | "");

            REQUIRE((t0 - base >= 10 * ((int)nrInitiated - (MO_METERVALUES_CACHE_MAXSIZE - (int)countProcessed)) && t0 - base <= 1 + 10 * ((int)nrInitiated - (MO_METERVALUES_CACHE_MAXSIZE - (int)countProcessed))));
        });


        loop();

        beginTransaction_authorized("mIdTag");

        base = model.getClock().now();
        auto trackMtime = mtime;

        loop();

        loopback.setConnected(false);

        //initiate 10 more MeterValues than can be cached
        for (unsigned long i = 1; i <= 10 + MO_METERVALUES_CACHE_MAXSIZE; i++) {
            mtime = trackMtime + i * 10 * 1000;
            loop();

            nrInitiated++;
        }

        loopback.setConnected(true);

        loop();

        REQUIRE(countProcessed == MO_METERVALUES_CACHE_MAXSIZE);

        endTransaction();

        loop();

    }

    SECTION("Drop MeterValues for silent tx") {

        loopback.setConnected(false);

        declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "PreBootTransactions", true)->setBool(true);

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

        configuration_save();

        unsigned int countProcessed = 0;

        setOnReceiveRequest("StartTransaction", [&countProcessed] (JsonObject) {
            countProcessed++;
        });

        int assignedTxId = -1;

        setOnSendConf("StartTransaction", [&assignedTxId] (JsonObject conf) {
            assignedTxId = conf["transactionId"];
        });

        setOnReceiveRequest("MeterValues", [&countProcessed, &assignedTxId] (JsonObject req) {
            countProcessed++;
        });

        setOnReceiveRequest("StopTransaction", [&countProcessed] (JsonObject) {
            REQUIRE(countProcessed == 2);
        });

        loop();

        auto trackMtime = mtime;

        beginTransaction_authorized("mIdTag");
        auto tx = getTransaction();

        loop();

        REQUIRE( getChargePointStatus() == ChargePointStatus_Charging );

        mtime = trackMtime + 10 * 1000;

        loop();

        endTransaction();

        loop();

        tx->setSilent();
        tx->commit();

        loopback.setConnected(true);

        loop();

        REQUIRE(countProcessed == 0);
    }

    SECTION("TxMsg retry behavior") {
        
        Timestamp base;

        addMeterValueInput([&base] () {
            //simulate 3600W consumption
            return getOcppContext()->getModel().getClock().now() - base;
        }, "Energy.Active.Import.Register");

        auto MeterValuesSampledDataString = declareConfiguration<const char*>("MeterValuesSampledData","", CONFIGURATION_FN);
        MeterValuesSampledDataString->setString("Energy.Active.Import.Register");

        auto MeterValueSampleIntervalInt = declareConfiguration<int>("MeterValueSampleInterval",0, CONFIGURATION_FN);
        MeterValueSampleIntervalInt->setInt(10);

        configuration_save();

        const size_t NUM_ATTEMPTS = 3;
        const int RETRY_INTERVAL_SECS = 3600;

        declareConfiguration<int>("TransactionMessageAttempts", 0)->setInt(NUM_ATTEMPTS);
        declareConfiguration<int>("TransactionMessageRetryInterval", 0)->setInt(RETRY_INTERVAL_SECS);

        unsigned int attemptNr = 0;

        getOcppContext()->getOperationRegistry().registerOperation("MeterValues", [&attemptNr] () {
            return new Ocpp16::CustomOperation("MeterValues",
                [&attemptNr] (JsonObject payload) {
                    //receive req
                    attemptNr++;
                },
                [] () {
                    //create conf
                    return createEmptyDocument();
                },
                [] () {
                    //ErrorCode for CALLERROR
                    return "InternalError";
                });});

        loop();

        auto trackMtime = mtime;
        base = model.getClock().now();

        beginTransaction("mIdTag");

        loop();

        mtime = trackMtime + 10 * 1000;

        loop();

        REQUIRE(attemptNr == 1);

        endTransaction();

        mtime = trackMtime + 20 * 1000;
        loop();
        REQUIRE(attemptNr == 1);

        mtime = trackMtime + 10 * 1000 + RETRY_INTERVAL_SECS * 1000;
        loop();
        REQUIRE(attemptNr == 2);

        mtime = trackMtime + 10 * 1000 + 2 * RETRY_INTERVAL_SECS * 1000;
        loop();
        REQUIRE(attemptNr == 2);

        mtime = trackMtime + 10 * 1000 + 3 * RETRY_INTERVAL_SECS * 1000;
        loop();
        REQUIRE(attemptNr == 3);

        mtime = trackMtime + 10 * 1000 + 7 * RETRY_INTERVAL_SECS * 1000;
        loop();
        REQUIRE(attemptNr == 3);
    }

    SECTION("TriggerMessage") {
        
        addMeterValueInput([] () {
            return 12345;
        }, "Energy.Active.Import.Register");

        auto MeterValuesSampledDataString = declareConfiguration<const char*>("MeterValuesSampledData","", CONFIGURATION_FN);
        MeterValuesSampledDataString->setString("Energy.Active.Import.Register");

        Timestamp base;

        bool checkProcessed = false;

        setOnReceiveRequest("MeterValues", [&base, &checkProcessed] (JsonObject payload) {
            int connectorId = payload["connectorId"] | -1;
            if (connectorId != 1) {
                return;
            }

            checkProcessed = true;

            Timestamp t0;
            t0.setTime(payload["meterValue"][0]["timestamp"] | "");

            REQUIRE( std::abs(t0 - base) <= 1 );
            REQUIRE( !strncmp(payload["meterValue"][0]["sampledValue"][0]["value"] | "", "12345", strlen("12345")) );
        });

        loop();

        base = model.getClock().now();

        loopback.sendTXT(TRIGGER_METERVALUES, sizeof(TRIGGER_METERVALUES) - 1);
        loop();

        REQUIRE(checkProcessed);

    }

    mocpp_deinitialize();
}
