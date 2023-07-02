#include <ArduinoOcpp.h>
#include <ArduinoOcpp/Core/Connection.h>
#include <ArduinoOcpp/Core/Context.h>
#include <ArduinoOcpp/Model/Model.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Core/SimpleRequestFactory.h>
#include <ArduinoOcpp/Operations/BootNotification.h>
#include <ArduinoOcpp/Operations/StatusNotification.h>
#include <ArduinoOcpp/Debug.h>
#include "./catch2/catch.hpp"
#include "./helpers/testHelper.h"

using namespace ArduinoOcpp;


TEST_CASE( "Transaction safety" ) {

    //initialize Context with dummy socket
    LoopbackConnection loopback;
    ocpp_initialize(loopback);

    ao_set_timer(custom_timer_cb);

    auto connectionTimeOut = declareConfiguration<int>("ConnectionTimeOut", 30, CONFIGURATION_FN);
        *connectionTimeOut = 30;

    SECTION("Basic transaction") {
        AO_DBG_DEBUG("Basic transaction");
        loop();
        startTransaction("mIdTag");
        loop();
        REQUIRE(ocppPermitsCharge());
        stopTransaction();
        loop();
        REQUIRE(!ocppPermitsCharge());

        ocpp_deinitialize();
    }

    SECTION("Managed transaction") {
        AO_DBG_DEBUG("Managed transaction");
        loop();
        setConnectorPluggedInput([] () {return true;});
        beginTransaction("mIdTag");
        loop();
        REQUIRE(ocppPermitsCharge());
        endTransaction();
        loop();
        REQUIRE(!ocppPermitsCharge());
        
        ocpp_deinitialize();
    }

    SECTION("Reset during transaction 01 - interrupt initiation") {
        AO_DBG_DEBUG("Reset during transaction 01 - interrupt initiation");
        setConnectorPluggedInput([] () {return false;});
        loop();
        beginTransaction("mIdTag");
        loop();
        ocpp_deinitialize(); //reset and jump to next section
    }

    SECTION("Reset during transaction 02 - interrupt initiation second time") {
        AO_DBG_DEBUG("Reset during transaction 02 - interrupt initiation second time");
        setConnectorPluggedInput([] () {return false;});
        loop();
        REQUIRE(!ocppPermitsCharge());
        ocpp_deinitialize();
    }

    SECTION("Reset during transaction 03 - interrupt running tx") {
        AO_DBG_DEBUG("Reset during transaction 03 - interrupt running tx");
        setConnectorPluggedInput([] () {return true;});
        loop();
        REQUIRE(ocppPermitsCharge());
        ocpp_deinitialize();
    }

    SECTION("Reset during transaction 04 - interrupt stopping tx") {
        AO_DBG_DEBUG("Reset during transaction 04 - interrupt stopping tx");
        setConnectorPluggedInput([] () {return true;});
        loop();
        REQUIRE(ocppPermitsCharge());
        endTransaction();
        ocpp_deinitialize();
    }

    SECTION("Reset during transaction 06 - check tx finished") {
        AO_DBG_DEBUG("Reset during transaction 06 - check tx finished");
        setConnectorPluggedInput([] () {return true;});
        loop();
        REQUIRE(!ocppPermitsCharge());
        ocpp_deinitialize();
    }

}
