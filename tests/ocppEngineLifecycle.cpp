#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include "./catch2/catch.hpp"
#include "./helpers/testHelper.h"

TEST_CASE( "Context lifecycle" ) {
    printf("\nRun %s\n",  "Context lifecycle");

    //set console output to the cpp console to display outputs
    //mocpp_set_console_out(cpp_console_out);

    //initialize Context with dummy socket
    MicroOcpp::LoopbackConnection loopback;
    mocpp_initialize(loopback);

    SECTION("OCPP is initialized"){
        REQUIRE( getOcppContext() );
    }

    mocpp_deinitialize();

    SECTION("OCPP is deinitialized"){
        REQUIRE( !( getOcppContext() ) );
    }
}
