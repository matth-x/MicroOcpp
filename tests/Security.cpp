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
#include <MicroOcpp/Operations/CustomOperation.h>
#include <MicroOcpp/Operations/SecurityEventNotification.h>
#include <MicroOcpp/Debug.h>
#include <catch2/catch.hpp>
#include "./helpers/testHelper.h"

#define BASE_TIME "2023-01-01T00:00:00.000Z"

using namespace MicroOcpp;


TEST_CASE( "Security" ) {
    printf("\nRun %s\n",  "Security");

    mocpp_set_timer(custom_timer_cb);

    //initialize Context with dummy socket
    LoopbackConnection loopback;
    auto filesystem = makeDefaultFilesystemAdapter(FilesystemOpt::Use_Mount_FormatOnFail);
    mocpp_initialize(loopback,
            ChargerCredentials(),
            filesystem,
            false,
            ProtocolVersion(2,0,1));

    SECTION("Manual SecurityEventNotification") {

        loop();

        MO_MEM_RESET();

        getOcppContext()->initiateRequest(makeRequest(new Ocpp201::SecurityEventNotification(
                "ReconfigurationOfSecurityParameters",
                getOcppContext()->getModel().getClock().now())));
        
        loop();

        MO_MEM_PRINT_STATS();
    }

    mocpp_deinitialize();
}

#endif //MO_ENABLE_V201
