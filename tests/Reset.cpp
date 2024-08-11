// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Transactions/TransactionService.h>
#include <MicroOcpp/Model/Reset/ResetService.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Operations/CustomOperation.h>
#include <MicroOcpp/Debug.h>
#include <catch2/catch.hpp>
#include "./helpers/testHelper.h"

#define BASE_TIME "2023-01-01T00:00:00.000Z"

using namespace MicroOcpp;


TEST_CASE( "Reset" ) {
    printf("\nRun %s\n",  "Reset");

    //initialize Context with dummy socket
    LoopbackConnection loopback;
    mocpp_initialize(loopback,
            ChargerCredentials("test-runner1234"),
            makeDefaultFilesystemAdapter(FilesystemOpt::Use_Mount_FormatOnFail),
            false,
            ProtocolVersion(2,0,1));

    auto context = getOcppContext();

    mocpp_set_timer(custom_timer_cb);

    // Register Reset handlers
    bool checkNotified [MO_NUM_EVSE] = {false};
    bool checkExecuted [MO_NUM_EVSE] = {false};

    setOnResetNotify([&checkNotified] (bool) {
        MO_DBG_DEBUG("Notify");
        checkNotified[0] = true;
        return true;
    });
    context->getModel().getResetServiceV201()->setExecuteReset([&checkExecuted] () {
        MO_DBG_DEBUG("Execute");
        checkExecuted[0] = true;
        return false; // Reset fails because we're not actually exiting the process
    });
    
    for (size_t i = 1; i < MO_NUM_EVSE; i++) {
        context->getModel().getResetServiceV201()->setNotifyReset([&checkNotified, i] (ResetType) {
            MO_DBG_DEBUG("Notify %zu", i);
            checkNotified[i] = true;
            return true;
        }, i);
        context->getModel().getResetServiceV201()->setExecuteReset([&checkExecuted, i] () {
            MO_DBG_DEBUG("Execute %zu", i);
            checkExecuted[i] = true;
            return true;
        }, i);
    }

    loop();

    SECTION("Schedule full charger Reset") {

        REQUIRE( context->getModel().getTransactionService()->getEvse(1)->getTransaction() == nullptr );
        REQUIRE( context->getModel().getTransactionService()->getEvse(2)->getTransaction() == nullptr );
        
        context->getModel().getTransactionService()->getEvse(1)->beginAuthorization("mIdToken");
        setConnectorPluggedInput([] () {return true;}, 1);
        setEvReadyInput([] () {return true;}, 1);
        setEvseReadyInput([] () {return true;}, 1);

        context->getModel().getTransactionService()->getEvse(2)->beginAuthorization("mIdToken2");
        setConnectorPluggedInput([] () {return true;}, 2);
        setEvReadyInput([] () {return true;}, 2);
        setEvseReadyInput([] () {return true;}, 2);

        loop();

        REQUIRE( ocppPermitsCharge(1) );
        REQUIRE( ocppPermitsCharge(2) );

        bool checkProcessed = false;

        auto resetRequest = makeRequest(new Ocpp16::CustomOperation(
                "Reset",
                [] () {
                    //create req
                    auto doc = makeMemJsonDoc("UnitTests", JSON_OBJECT_SIZE(1));
                    auto payload = doc->to<JsonObject>();
                    payload["type"] = "OnIdle";
                    return doc;},
                [&checkProcessed] (JsonObject payload) {
                    //receive conf
                    checkProcessed = true;

                    REQUIRE(!strcmp(payload["status"], "Scheduled"));
                }
        ));

        context->initiateRequest(std::move(resetRequest));

        loop();
        mtime += 30000; // Reset has some delays to ensure that the WS is not cut off immediately
        loop();

        REQUIRE(checkProcessed);

        for (size_t i = 0; i < MO_NUM_EVSE; i++) {
            REQUIRE( checkNotified[i] );
        }

        // Still scheduled
        REQUIRE( ocppPermitsCharge(1) );
        REQUIRE( ocppPermitsCharge(2) );

        context->getModel().getTransactionService()->getEvse(1)->endAuthorization("mIdToken");
        setConnectorPluggedInput([] () {return false;}, 1);
        setEvReadyInput([] () {return false;}, 1);
        setEvseReadyInput([] () {return false;}, 1);
        loop();

        // Still scheduled
        REQUIRE( !ocppPermitsCharge(1) );
        REQUIRE( ocppPermitsCharge(2) );

        REQUIRE( context->getModel().getConnector(1)->getStatus() == ChargePointStatus_Unavailable );

        context->getModel().getTransactionService()->getEvse(2)->endAuthorization("mIdToken");
        setConnectorPluggedInput([] () {return false;}, 2);
        setEvReadyInput([] () {return false;}, 2);
        setEvseReadyInput([] () {return false;}, 2);

        loop();
        mtime += 30000; // Reset has some delays to ensure that the WS is not cut off immediately
        loop();

        // Not scheduled anymore; execute Reset
        REQUIRE( !ocppPermitsCharge(1) );
        REQUIRE( !ocppPermitsCharge(2) );

        REQUIRE( checkExecuted[0] );

        // Technically, Reset failed at this point, because the program is still running. Check if connectors are Available agin
        REQUIRE( context->getModel().getConnector(1)->getStatus() == ChargePointStatus_Available );
        REQUIRE( context->getModel().getConnector(2)->getStatus() == ChargePointStatus_Available );
    }

    SECTION("Immediate full charger Reset") {

        context->getModel().getVariableService()->declareVariable<const char*>("TxCtrlr", "TxStartPoint", "")->setString("Authorized");

        REQUIRE( context->getModel().getTransactionService()->getEvse(1)->getTransaction() == nullptr );
        REQUIRE( context->getModel().getTransactionService()->getEvse(2)->getTransaction() == nullptr );
        
        context->getModel().getTransactionService()->getEvse(1)->beginAuthorization("mIdToken");

        context->getModel().getTransactionService()->getEvse(2)->beginAuthorization("mIdToken2");

        loop();

        REQUIRE( ocppPermitsCharge(1) );
        REQUIRE( ocppPermitsCharge(2) );

        bool checkProcessedTx = false;

        getOcppContext()->getOperationRegistry().registerOperation("TransactionEvent", [&checkProcessedTx] () {
            return new Ocpp16::CustomOperation("TransactionEvent",
                [&checkProcessedTx] (JsonObject payload) {
                    //process req
                    checkProcessedTx = true;

                    REQUIRE(!strcmp(payload["eventType"], "Ended"));
                    REQUIRE(!strcmp(payload["triggerReason"], "ResetCommand"));
                    REQUIRE(!strcmp(payload["transactionInfo"]["stoppedReason"], "ImmediateReset"));
                },
                [] () {
                    //create conf
                    return createEmptyDocument();
                });});

        bool checkProcessed = false;

        auto resetRequest = makeRequest(new Ocpp16::CustomOperation(
                "Reset",
                [] () {
                    //create req
                    auto doc = makeMemJsonDoc("UnitTests", JSON_OBJECT_SIZE(1));
                    auto payload = doc->to<JsonObject>();
                    payload["type"] = "Immediate";
                    return doc;},
                [&checkProcessed] (JsonObject payload) {
                    //receive conf
                    checkProcessed = true;

                    REQUIRE(!strcmp(payload["status"], "Accepted"));
                }
        ));

        context->initiateRequest(std::move(resetRequest));

        loop();
        mtime += 30000; // Reset has some delays to ensure that the WS is not cut off immediately
        loop();

        REQUIRE(checkProcessed);
        REQUIRE(checkProcessedTx);

        for (size_t i = 0; i < MO_NUM_EVSE; i++) {
            REQUIRE( checkNotified[i] );
        }

        // Stopped Tx
        REQUIRE( !ocppPermitsCharge(1) );
        REQUIRE( !ocppPermitsCharge(2) );

        REQUIRE( checkExecuted[0] );

        loop();

        // Technically, Reset failed at this point, because the program is still running. Check if connectors are Available agin
        REQUIRE( context->getModel().getConnector(1)->getStatus() == ChargePointStatus_Available );
        REQUIRE( context->getModel().getConnector(2)->getStatus() == ChargePointStatus_Available );
    }

    SECTION("Reject Reset") {
        
        context->getModel().getResetServiceV201()->setNotifyReset([&checkNotified] (ResetType) {
            MO_DBG_DEBUG("Reject Reset");
            checkNotified[2] = true;
            return false;
        }, 2);

        bool checkProcessed = false;

        auto resetRequest = makeRequest(new Ocpp16::CustomOperation(
                "Reset",
                [] () {
                    //create req
                    auto doc = makeMemJsonDoc("UnitTests", JSON_OBJECT_SIZE(1));
                    auto payload = doc->to<JsonObject>();
                    payload["type"] = "Immediate";
                    return doc;},
                [&checkProcessed] (JsonObject payload) {
                    //receive conf
                    checkProcessed = true;

                    REQUIRE(!strcmp(payload["status"], "Rejected"));
                }
        ));

        context->initiateRequest(std::move(resetRequest));

        loop();

        REQUIRE(checkProcessed);
        REQUIRE(checkNotified[2]);

        REQUIRE( context->getModel().getConnector(0)->getStatus() == ChargePointStatus_Available );
        REQUIRE( context->getModel().getConnector(1)->getStatus() == ChargePointStatus_Available );
        REQUIRE( context->getModel().getConnector(2)->getStatus() == ChargePointStatus_Available );
    }

    SECTION("Reset single EVSE") {

        bool checkProcessed = false;

        auto resetRequest = makeRequest(new Ocpp16::CustomOperation(
                "Reset",
                [] () {
                    //create req
                    auto doc = makeMemJsonDoc("UnitTests", JSON_OBJECT_SIZE(2));
                    auto payload = doc->to<JsonObject>();
                    payload["type"] = "OnIdle";
                    payload["evseId"] = 1;
                    return doc;},
                [&checkProcessed] (JsonObject payload) {
                    //receive conf
                    checkProcessed = true;

                    REQUIRE(!strcmp(payload["status"], "Accepted"));
                }
        ));

        context->initiateRequest(std::move(resetRequest));

        loop();

        REQUIRE(checkProcessed);
        REQUIRE(checkNotified[1]);

        REQUIRE( context->getModel().getConnector(1)->getStatus() == ChargePointStatus_Unavailable );
        REQUIRE( context->getModel().getConnector(2)->getStatus() == ChargePointStatus_Available );

        mtime += 30000; // Reset has some delays to ensure that the WS is not cut off immediately
        loop();

        REQUIRE(checkExecuted[1]);
        REQUIRE( context->getModel().getConnector(1)->getStatus() == ChargePointStatus_Available );
    }

    mocpp_deinitialize();
}

#endif // MO_ENABLE_V201
