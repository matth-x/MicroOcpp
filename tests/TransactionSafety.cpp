#include <ArduinoOcpp.h>
#include <ArduinoOcpp/Core/OcppSocket.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>
#include <ArduinoOcpp/MessagesV16/BootNotification.h>
#include <ArduinoOcpp/MessagesV16/StatusNotification.h>
#include <ArduinoOcpp/Debug.h>
#include "./catch2/catch.hpp"
#include "./helpers/testHelper.h"

using namespace ArduinoOcpp;


TEST_CASE( "Transaction safety" ) {

    //initialize OcppEngine with dummy socket
    OcppEchoSocket echoSocket;
    OCPP_initialize(echoSocket);

    ao_set_timer(custom_timer_cb);

    auto connectionTimeOut = declareConfiguration<int>("ConnectionTimeOut", 30, CONFIGURATION_FN);
        *connectionTimeOut = 30;

    bootNotification("dummy1234", "");

    SECTION("Basic transaction") {
        AO_DBG_DEBUG("Basic transaction");
        OCPP_loop();
        OCPP_loop();
        OCPP_loop();
        startTransaction("mIdTag");
        OCPP_loop();
        REQUIRE(ocppPermitsCharge());
        stopTransaction();
        OCPP_loop();
        REQUIRE(!ocppPermitsCharge());

        OCPP_deinitialize();
    }

    SECTION("Managed transaction") {
        AO_DBG_DEBUG("Managed transaction");
        OCPP_loop();
        OCPP_loop();
        OCPP_loop();
        setConnectorPluggedInput([] () {return true;});
        beginTransaction("mIdTag");
        OCPP_loop();
        REQUIRE(ocppPermitsCharge());
        endTransaction();
        OCPP_loop();
        REQUIRE(!ocppPermitsCharge());
        
        OCPP_deinitialize();
    }

    SECTION("Reset during transaction 01 - interrupt initiation") {
        AO_DBG_DEBUG("Reset during transaction 01 - interrupt initiation");
        setConnectorPluggedInput([] () {return false;});
        OCPP_loop();
        OCPP_loop();
        OCPP_loop();
        beginTransaction("mIdTag");
        OCPP_deinitialize(); //reset and jump to next section
    }

    SECTION("Reset during transaction 02 - interrupt initiation second time") {
        AO_DBG_DEBUG("Reset during transaction 02 - interrupt initiation second time");
        setConnectorPluggedInput([] () {return false;});
        OCPP_loop();
        OCPP_loop();
        OCPP_loop();
        REQUIRE(!ocppPermitsCharge());
        OCPP_deinitialize();
    }

    SECTION("Reset during transaction 03 - interrupt running tx") {
        AO_DBG_DEBUG("Reset during transaction 03 - interrupt running tx");
        setConnectorPluggedInput([] () {return true;});
        OCPP_loop();
        OCPP_loop();
        OCPP_loop();
        REQUIRE(ocppPermitsCharge());
        OCPP_deinitialize();
    }

    SECTION("Reset during transaction 04 - interrupt stopping tx") {
        AO_DBG_DEBUG("Reset during transaction 04 - interrupt stopping tx");
        setConnectorPluggedInput([] () {return true;});
        OCPP_loop();
        OCPP_loop();
        OCPP_loop();
        REQUIRE(ocppPermitsCharge());
        endTransaction();
        OCPP_deinitialize();
    }

    SECTION("Reset during transaction 06 - check tx finished") {
        AO_DBG_DEBUG("Reset during transaction 06 - check tx finished");
        setConnectorPluggedInput([] () {return true;});
        OCPP_loop();
        OCPP_loop();
        OCPP_loop();
        REQUIRE(!ocppPermitsCharge());
        OCPP_deinitialize();
    }

}
