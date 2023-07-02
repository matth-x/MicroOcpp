#include <ArduinoOcpp.h>
#include <ArduinoOcpp/Core/Connection.h>
#include "./catch2/catch.hpp"
#include "./helpers/testHelper.h"

TEST_CASE( "Context lifecycle" ) {

    //set console output to the cpp console to display outputs
    //ao_set_console_out(cpp_console_out);

    //initialize Context with dummy socket
    ArduinoOcpp::LoopbackConnection loopback;
    ocpp_initialize(loopback);

    SECTION("OCPP is initialized"){
        REQUIRE( getOcppContext() );
    }

    ocpp_deinitialize();

    SECTION("OCPP is deinitialized"){
        REQUIRE( !( getOcppContext() ) );
    }
}
