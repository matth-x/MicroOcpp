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
#include <MicroOcpp/Operations/CustomOperation.h>
#include <MicroOcpp/Debug.h>
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
                auto doc = makeMemJsonDoc("UnitTests", 2 * JSON_OBJECT_SIZE(1));
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

    mocpp_deinitialize();
}

#endif // MO_ENABLE_V201
