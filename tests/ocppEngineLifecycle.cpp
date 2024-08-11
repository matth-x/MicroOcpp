// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include <catch2/catch.hpp>
#include "./helpers/testHelper.h"

TEST_CASE( "Context lifecycle" ) {
    printf("\nRun %s\n",  "Context lifecycle");

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
