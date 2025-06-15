// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp.h>
#include <catch2/catch.hpp>
#include "./helpers/testHelper.h"

TEST_CASE( "Context lifecycle" ) {
    printf("\nRun %s\n",  "Context lifecycle");

    //initialize Context without any configs
    mo_initialize();

    SECTION("OCPP is initialized"){
        REQUIRE( mo_getApiContext() );
    }

    mo_deinitialize();

    SECTION("OCPP is deinitialized"){
        REQUIRE( !( mo_getApiContext() ) );
    }
}
