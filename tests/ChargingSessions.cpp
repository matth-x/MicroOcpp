#include <ArduinoOcpp.h>
#include <ArduinoOcpp/Core/OcppSocket.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>
#include <ArduinoOcpp/MessagesV16/BootNotification.h>
#include <ArduinoOcpp/MessagesV16/StatusNotification.h>
#include "./catch2/catch.hpp"

using namespace ArduinoOcpp;

unsigned long mtime = 10000;
unsigned long custom_timer_cb() {
    return mtime;
}

void loop() {
    mtime += 10000;
    OCPP_loop();
    mtime += 10000;
    OCPP_loop();
}

TEST_CASE( "Charging sessions - basic" ) {

    //initialize OcppEngine with dummy socket
    OcppEchoSocket echoSocket;
    OCPP_initialize(echoSocket);

    ao_set_timer(custom_timer_cb);
    
    std::array<const char*, 2> expectedSN {"Available", "Available"};
    std::array<bool, 2> checkedSN {false, false};
    registerCustomOcppMessage("StatusNotification", [] () {return new Ocpp16::StatusNotification();},
        [&checkedSN, &expectedSN] (JsonObject request) {
            int connectorId = request["connectorId"] | -1;
            checkedSN[connectorId] = !strcmp(request["status"] | "Invalid", expectedSN[connectorId]);
        });

    bootNotification("dummy1234", "");

    SECTION("Check idle state"){

        bool checkedBN = false;
        registerCustomOcppMessage("BootNotification", [] () {return new Ocpp16::BootNotification();},
            [&checkedBN] (JsonObject request) {
                checkedBN = !strcmp(request["chargePointModel"] | "Invalid", "dummy1234");
            });
        
        loop();
        loop();
        REQUIRE( checkedBN );
        REQUIRE( checkedSN[0] );
        REQUIRE( checkedSN[1] );
        REQUIRE( isAvailable() );
        REQUIRE( !getSessionIdTag() );
        REQUIRE( !ocppPermitsCharge() );
    }

    loop();
    loop();

    SECTION("StartTx directly"){
        startTransaction("mIdTag");
        loop();
        REQUIRE(ocppPermitsCharge());
    }

    SECTION("StartTx via session management - plug in first") {
        checkedSN[1] = false;
        expectedSN[1] = "Preparing";
        setConnectorPluggedSampler([] () {return true;});
        loop();
        REQUIRE(checkedSN[1]);

        checkedSN[1] = false;
        expectedSN[1] = "Charging";
        beginSession("mIdTag");
        loop();
        loop();
        REQUIRE(checkedSN[1]);
        REQUIRE(ocppPermitsCharge());
    }

    SECTION("StartTx via session management - authorization first") {
        auto connectionTimeOut = declareConfiguration<int>("ConnectionTimeOut", 100, CONFIGURATION_FN);
        *connectionTimeOut = 100;

        checkedSN[1] = false;
        expectedSN[1] = "Preparing";
        setConnectorPluggedSampler([] () {return false;});
        beginSession("mIdTag");
        loop();
        REQUIRE(checkedSN[1]);

        checkedSN[1] = false;
        expectedSN[1] = "Charging";
        setConnectorPluggedSampler([] () {return true;});
        loop();
        loop();
        REQUIRE(checkedSN[1]);
        REQUIRE(ocppPermitsCharge());
    }

    OCPP_deinitialize();

}