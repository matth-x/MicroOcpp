#include <ArduinoOcpp.h>
#include <ArduinoOcpp/Core/OcppSocket.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>
#include <ArduinoOcpp/MessagesV16/BootNotification.h>
#include <ArduinoOcpp/MessagesV16/StatusNotification.h>
#include "./catch2/catch.hpp"
#include "./helpers/testHelper.h"

using namespace ArduinoOcpp;


TEST_CASE( "Charging sessions" ) {

    //initialize OcppEngine with dummy socket
    OcppEchoSocket echoSocket;
    OCPP_initialize(echoSocket);

    auto engine = getOcppEngine();
    auto& checkMsg = engine->getOperationDeserializer();

    ao_set_timer(custom_timer_cb);

    auto connectionTimeOut = declareConfiguration<int>("ConnectionTimeOut", 30, CONFIGURATION_FN);
        *connectionTimeOut = 30;
    auto minimumStatusDuration = declareConfiguration<int>("MinimumStatusDuration", 0, CONFIGURATION_FN);
        *minimumStatusDuration = 0;
    
    std::array<const char*, 2> expectedSN {"Available", "Available"};
    std::array<bool, 2> checkedSN {false, false};
    checkMsg.registerOcppOperation("StatusNotification", [] () -> OcppMessage* {return new Ocpp16::StatusNotification(0, OcppEvseState::NOT_SET, MIN_TIME);});
    checkMsg.setOnRequest("StatusNotification",
        [&checkedSN, &expectedSN] (JsonObject request) {
            int connectorId = request["connectorId"] | -1;
            checkedSN[connectorId] = !strcmp(request["status"] | "Invalid", expectedSN[connectorId]);
        });

    bootNotification("dummy1234", "");

    SECTION("Check idle state"){

        bool checkedBN = false;
        checkMsg.registerOcppOperation("BootNotification", [engine] () -> OcppMessage* {return new Ocpp16::BootNotification(engine->getOcppModel(), std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(0)));});
        checkMsg.setOnRequest("BootNotification",
            [&checkedBN] (JsonObject request) {
                checkedBN = !strcmp(request["chargePointModel"] | "Invalid", "dummy1234");
            });
        
        loop();
        loop();
        REQUIRE( checkedBN );
        REQUIRE( checkedSN[0] );
        REQUIRE( checkedSN[1] );
        REQUIRE( isOperative() );
        REQUIRE( !getTransactionIdTag() );
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
            mtime += *connectionTimeOut * 1000;
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
            endTransaction("Local");
            loop();
            REQUIRE(checkedSN[1]);
            REQUIRE(!ocppPermitsCharge());
        }

        SECTION("via session management - deauthorize first") {
            expectedSN[1] = "Finishing";
            setConnectorPluggedInput([] () {return true;});
            endTransaction("Local");
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

    SECTION("Advanced session management") {
        bool connectorPlugged = false;
        setConnectorPluggedInput([&connectorPlugged] () {return connectorPlugged;});

        TxEnableState meterState = TxEnableState::Inactive;
        setTxBasedMeterInOut([&meterState] (TxTrigger) {return meterState;});

        TxEnableState lockState = TxEnableState::Inactive;
        setConnectorLockInOut([&lockState] (TxTrigger) {return lockState;});

        beginTransaction("mIdTag");
        loop();
        REQUIRE(!ocppPermitsCharge());

        connectorPlugged = true;
        loop();
        REQUIRE(!ocppPermitsCharge());
        
        meterState = TxEnableState::Active;
        loop();
        REQUIRE(!ocppPermitsCharge());
        
        meterState = TxEnableState::Pending;
        lockState = TxEnableState::Active;
        loop();
        REQUIRE(!ocppPermitsCharge());
        
        meterState = TxEnableState::Active;
        lockState = TxEnableState::Pending;
        loop();
        REQUIRE(!ocppPermitsCharge());

        checkedSN[1] = false;
        expectedSN[1] = "Charging";
        
        lockState = TxEnableState::Active;
        loop();
        REQUIRE(ocppPermitsCharge());
        REQUIRE(checkedSN[1]);

        checkedSN[1] = false;
        expectedSN[1] = "SuspendedEVSE";

        lockState = TxEnableState::Pending;
        loop();
        REQUIRE(!ocppPermitsCharge());
        REQUIRE(!getTransactionIdTag());
        REQUIRE(checkedSN[1]);

        checkedSN[1] = false;
        expectedSN[1] = "Finishing";

        meterState = TxEnableState::Inactive;
        lockState = TxEnableState::Inactive;
        loop();
        REQUIRE(checkedSN[1]);

        checkedSN[1] = false;
        expectedSN[1] = "Available";

        connectorPlugged = false;
        loop();
        REQUIRE(checkedSN[1]);
    }

    OCPP_deinitialize();

}