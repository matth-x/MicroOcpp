#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>
#include <MicroOcpp/Operations/BootNotification.h>
#include <MicroOcpp/Operations/StatusNotification.h>
#include <MicroOcpp/Operations/CustomOperation.h>
#include "./catch2/catch.hpp"
#include "./helpers/testHelper.h"

#define BASE_TIME "2023-01-01T00:00:00.000Z"

using namespace MicroOcpp;


TEST_CASE( "Charging sessions" ) {
    printf("\nRun %s\n",  "Charging sessions");

    //initialize Context with dummy socket
    LoopbackConnection loopback;
    mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));

    auto engine = getOcppContext();
    auto& checkMsg = engine->getOperationRegistry();

    mocpp_set_timer(custom_timer_cb);

    auto connectionTimeOutInt = declareConfiguration<int>("ConnectionTimeOut", 30, CONFIGURATION_FN);
    connectionTimeOutInt->setInt(30);
    auto minimumStatusDurationInt = declareConfiguration<int>("MinimumStatusDuration", 0, CONFIGURATION_FN);
    minimumStatusDurationInt->setInt(0);
    
    std::array<const char*, 2> expectedSN {"Available", "Available"};
    std::array<bool, 2> checkedSN {false, false};
    checkMsg.registerOperation("StatusNotification", [] () -> Operation* {return new Ocpp16::StatusNotification(0, ChargePointStatus::NOT_SET, MIN_TIME);});
    checkMsg.setOnRequest("StatusNotification",
        [&checkedSN, &expectedSN] (JsonObject request) {
            int connectorId = request["connectorId"] | -1;
            if (connectorId == 0 || connectorId == 1) { //only test single connector case here
                checkedSN[connectorId] = !strcmp(request["status"] | "Invalid", expectedSN[connectorId]);
            }
        });

    SECTION("Check idle state"){

        bool checkedBN = false;
        checkMsg.registerOperation("BootNotification", [engine] () -> Operation* {return new Ocpp16::BootNotification(engine->getModel(), std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(0)));});
        checkMsg.setOnRequest("BootNotification",
            [&checkedBN] (JsonObject request) {
                checkedBN = !strcmp(request["chargePointModel"] | "Invalid", "test-runner1234");
            });
        
        REQUIRE( !isOperative() ); //not operative before reaching loop stage

        loop();
        loop();
        REQUIRE( checkedBN );
        REQUIRE( checkedSN[0] );
        REQUIRE( checkedSN[1] );
        REQUIRE( isOperative() );
        REQUIRE( !getTransaction() );
        REQUIRE( !ocppPermitsCharge() );
    }

    loop();

    SECTION("StartTx") {
        SECTION("StartTx directly"){
            startTransaction("mIdTag");
            loop();
            REQUIRE(ocppPermitsCharge());
        }

        SECTION("StartTx via session management - plug in first") {
            expectedSN[1] = "Preparing";
            setConnectorPluggedInput([] () {return true;});
            loop();
            REQUIRE(checkedSN[1]);

            checkedSN[1] = false;
            expectedSN[1] = "Charging";
            beginTransaction("mIdTag");
            loop();
            REQUIRE(checkedSN[1]);
            REQUIRE(ocppPermitsCharge());
        }

        SECTION("StartTx via session management - authorization first") {

            expectedSN[1] = "Preparing";
            setConnectorPluggedInput([] () {return false;});
            beginTransaction("mIdTag");
            loop();
            REQUIRE(checkedSN[1]);

            checkedSN[1] = false;
            expectedSN[1] = "Charging";
            setConnectorPluggedInput([] () {return true;});
            loop();
            REQUIRE(checkedSN[1]);
            REQUIRE(ocppPermitsCharge());
        }

        SECTION("StartTx via session management - no plug") {
            expectedSN[1] = "Charging";
            beginTransaction("mIdTag");
            loop();
            REQUIRE(checkedSN[1]);
        }

        SECTION("StartTx via session management - ConnectionTimeOut") {
            expectedSN[1] = "Preparing";
            setConnectorPluggedInput([] () {return false;});
            beginTransaction("mIdTag");
            loop();
            REQUIRE(checkedSN[1]);

            checkedSN[1] = false;
            expectedSN[1] = "Available";
            mtime += connectionTimeOutInt->getInt() * 1000;
            loop();
            REQUIRE(checkedSN[1]);
        }

        loop();
        if (ocppPermitsCharge()) {
            stopTransaction();
        }
        loop();
    }

    SECTION("StopTx") {
        startTransaction("mIdTag");
        loop();
        expectedSN[1] = "Available";

        SECTION("directly") {
            stopTransaction();
            loop();
            REQUIRE(checkedSN[1]);
            REQUIRE(!ocppPermitsCharge());
        }

        SECTION("via session management - deauthorize") {
            endTransaction();
            loop();
            REQUIRE(checkedSN[1]);
            REQUIRE(!ocppPermitsCharge());
        }

        SECTION("via session management - deauthorize first") {
            expectedSN[1] = "Finishing";
            setConnectorPluggedInput([] () {return true;});
            endTransaction();
            loop();
            REQUIRE(checkedSN[1]);
            REQUIRE(!ocppPermitsCharge());

            checkedSN[1] = false;
            expectedSN[1] = "Available";
            setConnectorPluggedInput([] () {return false;});
            loop();
            REQUIRE(checkedSN[1]);
            REQUIRE(!ocppPermitsCharge());
        }

        SECTION("via session management - plug out") {
            setConnectorPluggedInput([] () {return false;});
            loop();
            REQUIRE(checkedSN[1]);
            REQUIRE(!ocppPermitsCharge());
        }

        if (ocppPermitsCharge()) {
            stopTransaction();
        }
        loop();
    }

    SECTION("Preboot transactions - tx before BootNotification") {
        mocpp_deinitialize();

        loopback.setConnected(false);
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));

        declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "PreBootTransactions", true, CONFIGURATION_FN)->setBool(true);
        configuration_save();

        loop();

        beginTransaction_authorized("mIdTag");

        loop();

        REQUIRE(isTransactionRunning());

        mtime += 3600 * 1000; //transaction duration ~1h

        endTransaction();

        loop();

        mtime += 3600 * 1000; //set base time one hour later

        bool checkStartProcessed = false;

        getOcppContext()->getModel().getClock().setTime(BASE_TIME);
        Timestamp basetime = Timestamp();
        basetime.setTime(BASE_TIME);

        getOcppContext()->getOperationRegistry().setOnRequest("StartTransaction", 
            [&checkStartProcessed, basetime] (JsonObject payload) {
                checkStartProcessed = true;
                Timestamp timestamp;
                timestamp.setTime(payload["timestamp"].as<const char*>());                

                auto adjustmentDelay = basetime - timestamp;
                REQUIRE((adjustmentDelay > 2 * 3600 - 10 && adjustmentDelay < 2 * 3600 + 10));
            });
        
        bool checkStopProcessed = false;

        getOcppContext()->getOperationRegistry().setOnRequest("StopTransaction", 
            [&checkStopProcessed, basetime] (JsonObject payload) {
                checkStopProcessed = true;
                Timestamp timestamp;
                timestamp.setTime(payload["timestamp"].as<const char*>());                

                auto adjustmentDelay = basetime - timestamp;
                REQUIRE((adjustmentDelay > 3600 - 10 && adjustmentDelay < 3600 + 10));
            });
        
        loopback.setConnected(true);
        loop();

        REQUIRE(checkStartProcessed);
        REQUIRE(checkStopProcessed);
    }

    SECTION("Preboot transactions - lose StartTx timestamp") {

        mocpp_deinitialize();

        loopback.setConnected(false);
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));

        declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "PreBootTransactions", true, CONFIGURATION_FN)->setBool(true);
        configuration_save();

        loop();

        beginTransaction_authorized("mIdTag");
        loop();

        REQUIRE(isTransactionRunning());

        mocpp_deinitialize();

        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));

        declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "PreBootTransactions", true, CONFIGURATION_FN)->setBool(true);
        configuration_save();

        bool checkProcessed = false;

        getOcppContext()->getOperationRegistry().setOnRequest("StartTransaction", [&checkProcessed] (JsonObject) {
            checkProcessed = true;
        });

        getOcppContext()->getOperationRegistry().setOnRequest("StopTransaction", [&checkProcessed] (JsonObject) {
            checkProcessed = true;
        });

        loopback.setConnected(true);

        loop();

        REQUIRE(!isTransactionRunning());
        REQUIRE(!checkProcessed);
    }

    SECTION("Preboot transactions - lose StopTx timestamp") {
        
        const char *starTxTimestampStr = "2023-02-01T00:00:00.000Z";
        getOcppContext()->getModel().getClock().setTime(starTxTimestampStr);

        beginTransaction_authorized("mIdTag");

        loop();

        REQUIRE(isTransactionRunning());

        mocpp_deinitialize();

        loopback.setConnected(false);
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));

        declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "PreBootTransactions", true, CONFIGURATION_FN)->setBool(true);
        configuration_save();

        loop();

        REQUIRE(isTransactionRunning());

        endTransaction();

        loop();

        REQUIRE(!isTransactionRunning());

        mocpp_deinitialize();

        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));

        declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "PreBootTransactions", true, CONFIGURATION_FN)->setBool(true);
        configuration_save();

        bool checkProcessed = false;

        getOcppContext()->getOperationRegistry().setOnRequest("StopTransaction",
            [&checkProcessed, starTxTimestampStr] (JsonObject payload) {
                checkProcessed = true;
                Timestamp timestamp;
                timestamp.setTime(payload["timestamp"].as<const char*>());

                Timestamp starTxTimestamp = Timestamp();
                starTxTimestamp.setTime(starTxTimestampStr);

                auto adjustmentDelay = timestamp - starTxTimestamp;
                REQUIRE(adjustmentDelay == 1);
            });

        loopback.setConnected(true);

        loop();

        REQUIRE(checkProcessed);
    }

    SECTION("Set Unavaible"){

        beginTransaction("mIdTag");

        loop();

        auto connector = getOcppContext()->getModel().getConnector(1);
        REQUIRE(connector->getStatus() == ChargePointStatus::Charging);
        REQUIRE(isOperative());

        bool checkProcessed = false;

        auto changeAvailability = makeRequest(new Ocpp16::CustomOperation(
                "ChangeAvailability",
                [] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(2)));
                    auto payload = doc->to<JsonObject>();
                    payload["connectorId"] = 1;
                    payload["type"] = "Inoperative";
                    return doc;},
                [&checkProcessed] (JsonObject payload) {
                    //receive conf
                    checkProcessed = true;

                    REQUIRE(!strcmp(payload["status"], "Scheduled"));}));

        getOcppContext()->initiateRequest(std::move(changeAvailability));

        loop();

        REQUIRE(checkProcessed);
        REQUIRE(connector->getStatus() == ChargePointStatus::Charging);
        REQUIRE(isOperative());

        mocpp_deinitialize();

        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        connector = getOcppContext()->getModel().getConnector(1);

        loop();

        REQUIRE(connector->getStatus() == ChargePointStatus::Charging);
        REQUIRE(isOperative());

        endTransaction();

        loop();

        REQUIRE(connector->getStatus() == ChargePointStatus::Unavailable);
        REQUIRE(!isOperative());

        connector->setAvailability(true);

        REQUIRE(connector->getStatus() == ChargePointStatus::Available);
        REQUIRE(isOperative());
    }

    mocpp_deinitialize();

}
