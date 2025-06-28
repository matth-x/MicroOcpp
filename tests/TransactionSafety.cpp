// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Debug.h>
#include <catch2/catch.hpp>
#include "./helpers/testHelper.h"

using namespace MicroOcpp;


TEST_CASE( "Transaction safety" ) {
    printf("\nRun %s\n",  "Transaction safety");

    //initialize Context without any configs
    mo_initialize();

    mo_getContext()->setTicksCb(custom_timer_cb);

    LoopbackConnection loopback;
    mo_getContext()->setConnection(&loopback);

    auto ocppVersion = GENERATE(MO_OCPP_V16, MO_OCPP_V201);
    mo_setOcppVersion(ocppVersion);

    mo_setup();

    mo_setVarConfigInt(mo_getApiContext(), "TxCtrlr", "EVConnectionTimeOut", "ConnectionTimeOut", 30);

    if (ocppVersion == MO_OCPP_V201) {
        mo_setVarConfigString(mo_getApiContext(), "TxCtrlr", "TxStartPoint", NULL, "PowerPathClosed");
        mo_setVarConfigString(mo_getApiContext(), "TxCtrlr", "TxStopPoint", NULL, "PowerPathClosed");
    }

    SECTION("Clean up files") {
        auto filesystem = mo_getFilesystem();
        FilesystemUtils::removeByPrefix(filesystem, "");
    }

    SECTION("Managed transaction") {
        MO_DBG_DEBUG("Managed transaction");
        loop();
        mo_setConnectorPluggedInput([] () {return true;});
        mo_beginTransaction("mIdTag");
        loop();
        REQUIRE(mo_ocppPermitsCharge());
        mo_endTransaction("mIdTag", NULL);
        loop();
        REQUIRE(!mo_ocppPermitsCharge());
    }

    SECTION("Reset during transaction 01 - interrupt initiation") {
        MO_DBG_DEBUG("Reset during transaction 01 - interrupt initiation");
        mo_setConnectorPluggedInput([] () {return false;});
        loop();
        mo_beginTransaction("mIdTag");
        loop();
    }

    SECTION("Reset during transaction 02 - interrupt initiation second time") {
        MO_DBG_DEBUG("Reset during transaction 02 - interrupt initiation second time");
        mo_setConnectorPluggedInput([] () {return false;});
        loop();
        REQUIRE(!mo_ocppPermitsCharge());
    }

    SECTION("Reset during transaction 03 - interrupt running tx") {
        MO_DBG_DEBUG("Reset during transaction 03 - interrupt running tx");
        mo_setConnectorPluggedInput([] () {return true;});
        loop();
        REQUIRE(mo_ocppPermitsCharge());
    }

    SECTION("Reset during transaction 04 - interrupt stopping tx") {
        MO_DBG_DEBUG("Reset during transaction 04 - interrupt stopping tx");
        mo_setConnectorPluggedInput([] () {return true;});
        loop();
        REQUIRE(mo_ocppPermitsCharge());
        mo_endTransaction("mIdTag", NULL);
    }

    SECTION("Reset during transaction 06 - check tx finished") {
        MO_DBG_DEBUG("Reset during transaction 06 - check tx finished");
        mo_setConnectorPluggedInput([] () {return true;});
        loop();
        REQUIRE(!mo_ocppPermitsCharge());
    }

    mo_deinitialize();
}
