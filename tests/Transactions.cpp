#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Transactions/TransactionService.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>
#include <MicroOcpp/Operations/CustomOperation.h>
#include "./catch2/catch.hpp"
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

    loop();

    SECTION("Basic transaction") {

        setConnectorPluggedInput([] () {return true;});
        setEvReadyInput([] () {return true;});
        setEvseReadyInput([] () {return true;});

        context->getModel().getTransactionService()->getEvse(1)->beginAuthorization("mIdToken");

        loop();

        setConnectorPluggedInput([] () {return false;});
        setEvReadyInput([] () {return false;});
        setEvseReadyInput([] () {return false;});

        context->getModel().getTransactionService()->getEvse(1)->endAuthorization();

        loop();
    }

    mocpp_deinitialize();
}

#endif // MO_ENABLE_V201
