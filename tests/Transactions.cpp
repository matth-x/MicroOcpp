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
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Operations/CustomOperation.h>
#include <MicroOcpp/Debug.h>
#include <MicroOcpp/Core/Memory.h>
#include <catch2/catch.hpp>
#include "./helpers/testHelper.h"

#define BASE_TIME "2023-01-01T00:00:00.000Z"

using namespace MicroOcpp;


TEST_CASE( "Transactions" ) {
    printf("\nRun %s\n",  "Transactions");

    //initialize Context with dummy socket
    LoopbackConnection loopback;
    mocpp_initialize(loopback,
            ChargerCredentials("test-runner1234"),
            makeDefaultFilesystemAdapter(FilesystemOpt::Use_Mount_FormatOnFail),
            false,
            ProtocolVersion(2,0,1));

    auto context = getOcppContext();
    auto& checkMsg = context->getOperationRegistry();

    mocpp_set_timer(custom_timer_cb);

    getOcppContext()->getOperationRegistry().registerOperation("Authorize", [] () {
        return new Ocpp16::CustomOperation("Authorize",
            [] (JsonObject) {}, //ignore req
            [] () {
                //create conf
                auto doc = makeJsonDoc("UnitTests", 2 * JSON_OBJECT_SIZE(1));
                auto payload = doc->to<JsonObject>();
                payload["idTokenInfo"]["status"] = "Accepted";
                return doc;
            });});

    loop();

    SECTION("Basic transaction") {

        REQUIRE( context->getModel().getTransactionService()->getEvse(1)->getTransaction() == nullptr );

        MO_DBG_DEBUG("plug EV");
        setConnectorPluggedInput([] () {return true;});

        loop();

        MO_DBG_DEBUG("authorize");
        context->getModel().getTransactionService()->getEvse(1)->beginAuthorization("mIdToken");

        loop();

        MO_DBG_DEBUG("EV requests charge");
        setEvReadyInput([] () {return true;});

        loop();

        MO_DBG_DEBUG("power circuit closed");
        setEvseReadyInput([] () {return true;});

        loop();

        REQUIRE( context->getModel().getTransactionService()->getEvse(1)->getTransaction()->started );
        REQUIRE( !context->getModel().getTransactionService()->getEvse(1)->getTransaction()->stopped );

        MO_DBG_DEBUG("EV idle");
        setEvReadyInput([] () {return false;});

        loop();

        MO_DBG_DEBUG("power circuit opened");
        setEvseReadyInput([] () {return false;});

        loop();

        MO_DBG_DEBUG("deauthorize");
        context->getModel().getTransactionService()->getEvse(1)->endAuthorization("mIdToken");

        loop();
        
        MO_DBG_DEBUG("unplug EV");
        setConnectorPluggedInput([] () {return false;});

        loop();

        REQUIRE( (context->getModel().getTransactionService()->getEvse(1)->getTransaction() == nullptr || 
                  context->getModel().getTransactionService()->getEvse(1)->getTransaction()->stopped));
    }

    SECTION("UC E01 - S5") {

        //scenario preparation

        REQUIRE( context->getModel().getTransactionService()->getEvse(1)->getTransaction() == nullptr );
        REQUIRE( getChargePointStatus() == ChargePointStatus_Available );

        getOcppContext()->getModel().getVariableService()->declareVariable<const char*>("TxCtrlr", "TxStartPoint", "")->setString("PowerPathClosed");
        getOcppContext()->getModel().getVariableService()->declareVariable<const char*>("TxCtrlr", "TxStopPoint", "")->setString("PowerPathClosed");

        setConnectorPluggedInput([] () {return false;});

        loop();

        MO_MEM_RESET();

        //run scenario

        setConnectorPluggedInput([] () {return true;});
        loop();
        REQUIRE( context->getModel().getTransactionService()->getEvse(1)->getTransaction() == nullptr );
        REQUIRE( getChargePointStatus() == ChargePointStatus_Occupied );

        context->getModel().getTransactionService()->getEvse(1)->beginAuthorization("mIdToken");
        loop();
        REQUIRE( context->getModel().getTransactionService()->getEvse(1)->getTransaction() != nullptr );
        REQUIRE( context->getModel().getTransactionService()->getEvse(1)->getTransaction()->started );
        REQUIRE( !context->getModel().getTransactionService()->getEvse(1)->getTransaction()->stopped );
        REQUIRE( getChargePointStatus() == ChargePointStatus_Occupied );

        MO_DBG_INFO("Memory requirements UC E01 - S5:");

        MO_MEM_PRINT_STATS();
    }

    SECTION("UC J02") {

        //scenario preparation

        REQUIRE( context->getModel().getTransactionService()->getEvse(1)->getTransaction() == nullptr );
        REQUIRE( getChargePointStatus() == ChargePointStatus_Available );

        getOcppContext()->getModel().getVariableService()->declareVariable<const char*>("TxCtrlr", "TxStartPoint", "")->setString("PowerPathClosed");
        getOcppContext()->getModel().getVariableService()->declareVariable<const char*>("TxCtrlr", "TxStopPoint", "")->setString("PowerPathClosed");
        
        getOcppContext()->getModel().getVariableService()->declareVariable<const char*>("SampledDataCtrlr", "SampledDataTxStartedMeasurands", "")->setString("Energy.Active.Import.Register");
        getOcppContext()->getModel().getVariableService()->declareVariable<const char*>("SampledDataCtrlr", "SampledDataTxUpdatedMeasurands", "")->setString("Power.Active.Import");
        getOcppContext()->getModel().getVariableService()->declareVariable<int>("SampledDataCtrlr", "TxUpdatedInterval", 0)->setInt(60);
        getOcppContext()->getModel().getVariableService()->declareVariable<const char*>("SampledDataCtrlr", "SampledDataTxEndedMeasurands", "")->setString("Current.Import");
        getOcppContext()->getModel().getVariableService()->declareVariable<int>("SampledDataCtrlr", "TxEndededInterval", 0)->setInt(100);

        setConnectorPluggedInput([] () {return false;});
        setEnergyMeterInput([] () {return 100;});
        setPowerMeterInput([] () {return 200;});
        addMeterValueInput([] () {return 30;}, "Current.Import", "A");

        Timestamp tStart, tUpdated, tEnded;

        setOnReceiveRequest("TransactionEvent", [&tStart, &tUpdated, &tEnded] (JsonObject request) {
            const char *eventType = request["eventType"] | (const char*)nullptr;
            bool eventTypeError = false;
            if (!strcmp(eventType, "Started")) {
                tStart = getOcppContext()->getModel().getClock().now();

                REQUIRE( request["meterValue"].as<JsonArray>().size() >= 1 );

                Timestamp tMv;
                tMv.setTime(request["meterValue"][0]["timestamp"]);
                REQUIRE( std::abs(tStart - tMv) <= 1);

                REQUIRE( request["meterValue"][0]["sampledValue"].as<JsonArray>().size() >= 1 );

                REQUIRE( !strcmp(request["meterValue"][0]["sampledValue"][0]["measurand"] | "_Undefined", "Energy.Active.Import.Register") );
                REQUIRE( !strcmp(request["meterValue"][0]["sampledValue"][0]["measurand"] | "_Undefined", "Energy.Active.Import.Register") );
            } else if (!strcmp(eventType, "Updated")) {
                tUpdated = getOcppContext()->getModel().getClock().now();
                
            } else if (!strcmp(eventType, "Ended")) {
                tEnded = getOcppContext()->getModel().getClock().now();
                
            } else {
                eventTypeError = true;
            }
            REQUIRE( !eventTypeError );
        });

        loop();

        MO_MEM_RESET();

        //run scenario

        setConnectorPluggedInput([] () {return true;});
        context->getModel().getTransactionService()->getEvse(1)->beginAuthorization("mIdToken");
        loop();
        REQUIRE( context->getModel().getTransactionService()->getEvse(1)->getTransaction() != nullptr );
        REQUIRE( context->getModel().getTransactionService()->getEvse(1)->getTransaction()->started );
        REQUIRE( !context->getModel().getTransactionService()->getEvse(1)->getTransaction()->stopped );
        REQUIRE( getChargePointStatus() == ChargePointStatus_Occupied );

        MO_DBG_INFO("Memory requirements UC E01 - S5:");

        MO_MEM_PRINT_STATS();
    }

    mocpp_deinitialize();
}

#endif // MO_ENABLE_V201
