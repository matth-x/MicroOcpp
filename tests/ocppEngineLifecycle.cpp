#include <ArduinoOcpp.h>
#include <ArduinoOcpp/Core/OcppSocket.h>
#include "./catch2/catch.hpp"
#include "./helpers/testHelper.h"

TEST_CASE( "OcppEngine lifecycle" ) {

    //set console output to the cpp console to display outputs
    ao_set_console_out(cpp_console_out);

    //initialize OcppEngine with dummy socket
    ArduinoOcpp::OcppEchoSocket echoSocket;
    OCPP_initialize(echoSocket);

    SECTION("OCPP is initialized"){
        REQUIRE( getOcppEngine() );
    }

    SECTION("OCPP is deinitialized"){
        OCPP_deinitialize();
        REQUIRE( !( getOcppEngine() ) );
    }
}