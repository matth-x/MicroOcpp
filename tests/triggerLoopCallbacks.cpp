#include <ArduinoOcpp.h>
#include <ArduinoOcpp/Core/OcppSocket.h>
#include "./catch2/catch.hpp"
#include "./testHelper.h"

#include <iostream>

TEST_CASE( "Trigger all callback functions in the Ocpp Loop" ) {
    //set console output to the cpp console to display outputs
    ao_set_console_out(cpp_console_out);
    //initialize OcppEngine with dummy socket
    ArduinoOcpp::OcppEchoSocket echoSocket;
    OCPP_initialize(echoSocket);

    SECTION("setOnChargingRateLimitChange"){

        bool hasRun = false;

        setOnChargingRateLimitChange([&hasRun](float limit) {
            hasRun = true;
            std::cout << "Has run!!!!" ;
        });
        OCPP_loop();

        REQUIRE( hasRun );
    }
}