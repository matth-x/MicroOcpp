// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Core/Request.h>
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

    getOcppContext()->getMessageService().registerOperation("Authorize", [] () {
        return new v16::CustomOperation("Authorize",
            [] (JsonObject) {}, //ignore req
            [] () {
                //create conf
                auto doc = makeJsonDoc("UnitTests", 2 * JSON_OBJECT_SIZE(1));
                auto payload = doc->to<JsonObject>();
                payload["idTokenInfo"]["status"] = "Accepted";
                return doc;
            });});

    getOcppContext()->getMessageService().registerOperation("TransactionEvent", [] () {
        return new v16::CustomOperation("TransactionEvent",
            [] (JsonObject) {}, //ignore req
            [] () {
                //create conf
                auto doc = makeJsonDoc("UnitTests", 2 * JSON_OBJECT_SIZE(1));
                auto payload = doc->to<JsonObject>();
                payload["idTokenInfo"]["status"] = "Accepted";
                return doc;
            });});

    // Register Reset handlers
    bool checkNotified [MO_NUM_EVSEID] = {false};
    bool checkExecuted [MO_NUM_EVSEID] = {false};

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
    
    for (size_t i = 1; i < MO_NUM_EVSEID; i++) {
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

    SECTION("B11 - Reset - Without ongoing transaction") {

        MO_MEM_RESET();

        bool checkProcessed = false;

        auto resetRequest = makeRequest(new v16::CustomOperation(
                "Reset",
                [] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(1));
                    auto payload = doc->to<JsonObject>();
                    payload["type"] = "OnIdle";
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

        for (size_t i = 0; i < MO_NUM_EVSEID; i++) {
            REQUIRE( checkNotified[i] );
        }

        MO_MEM_PRINT_STATS();
    }

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

        auto resetRequest = makeRequest(new v16::CustomOperation(
                "Reset",
                [] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(1));
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

        for (size_t i = 0; i < MO_NUM_EVSEID; i++) {
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

        //REQUIRE( getChargePointStatus(1) == ChargePointStatus_Unavailable ); //change: Reset doesn't lead to Unavailable state

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
        REQUIRE( getChargePointStatus(1) == ChargePointStatus_Available );
        REQUIRE( getChargePointStatus(2) == ChargePointStatus_Available );
    }

    SECTION("Immediate full charger Reset") {

        context->getModel().getVariableService()->declareVariable<const char*>("TxCtrlr", "TxStartPoint", "")->setString("Authorized");

        REQUIRE( context->getModel().getTransactionService()->getEvse(1)->getTransaction() == nullptr );
        REQUIRE( context->getModel().getTransactionService()->getEvse(2)->getTransaction() == nullptr );
        
        context->getModel().getTransactionService()->getEvse(1)->beginAuthorization("mIdToken");

        context->getModel().getTransactionService()->getEvse(2)->beginAuthorization("mIdToken2");

        loop();

        MO_MEM_RESET();

        REQUIRE( ocppPermitsCharge(1) );
        REQUIRE( ocppPermitsCharge(2) );

        bool checkProcessedTx = false;

        getOcppContext()->getMessageService().registerOperation("TransactionEvent", [&checkProcessedTx] () {
            return new v16::CustomOperation("TransactionEvent",
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

        auto resetRequest = makeRequest(new v16::CustomOperation(
                "Reset",
                [] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(1));
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

        for (size_t i = 0; i < MO_NUM_EVSEID; i++) {
            REQUIRE( checkNotified[i] );
        }

        // Stopped Tx
        REQUIRE( !ocppPermitsCharge(1) );
        REQUIRE( !ocppPermitsCharge(2) );

        REQUIRE( checkExecuted[0] );

        MO_MEM_PRINT_STATS();

        loop();

        // Technically, Reset failed at this point, because the program is still running. Check if connectors are Available agin
        REQUIRE( getChargePointStatus(1) == ChargePointStatus_Available );
        REQUIRE( getChargePointStatus(2) == ChargePointStatus_Available );
    }

    SECTION("Reject Reset") {
        
        context->getModel().getResetServiceV201()->setNotifyReset([&checkNotified] (ResetType) {
            MO_DBG_DEBUG("Reject Reset");
            checkNotified[2] = true;
            return false;
        }, 2);

        bool checkProcessed = false;

        auto resetRequest = makeRequest(new v16::CustomOperation(
                "Reset",
                [] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(1));
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

        REQUIRE( getChargePointStatus(0) == ChargePointStatus_Available );
        REQUIRE( getChargePointStatus(1) == ChargePointStatus_Available );
        REQUIRE( getChargePointStatus(2) == ChargePointStatus_Available );
    }

    SECTION("Reset single EVSE") {

        bool checkProcessed = false;

        auto resetRequest = makeRequest(new v16::CustomOperation(
                "Reset",
                [] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(2));
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

        //REQUIRE( getChargePointStatus(1) == ChargePointStatus_Unavailable ); //change: Reset doesn't lead to Unavailable state
        REQUIRE( getChargePointStatus(2) == ChargePointStatus_Available );

        mtime += 30000; // Reset has some delays to ensure that the WS is not cut off immediately
        loop();

        REQUIRE(checkExecuted[1]);
        REQUIRE( getChargePointStatus(1) == ChargePointStatus_Available );
    }

    mocpp_deinitialize();
}

#endif //MO_ENABLE_V201
