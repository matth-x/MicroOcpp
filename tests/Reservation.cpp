#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include "./catch2/catch.hpp"
#include "./helpers/testHelper.h"

#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Operations/CustomOperation.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>

#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Model/Reservation/ReservationService.h>


#define BASE_TIME "2023-01-01T00:00:00.000Z"

using namespace MicroOcpp;

TEST_CASE( "Reservation" ) {
    printf("\nRun %s\n",  "Reservation");

    //clean state
    auto filesystem = makeDefaultFilesystemAdapter(FilesystemOpt::Use_Mount_FormatOnFail);
    FilesystemUtils::remove_if(filesystem, [] (const char*) {return true;});

    //initialize Context with dummy socket
    LoopbackConnection loopback;

    mocpp_set_timer(custom_timer_cb);

    SECTION("Basic reservation") {
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        auto& model = getOcppContext()->getModel();
        model.getClock().setTime(BASE_TIME);

        loop();

        auto connector = model.getConnector(1);
        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        auto rService = model.getReservationService();
        REQUIRE( rService );

        //set reservation
        int reservationId = 1;
        unsigned int connectorId = 1;
        Timestamp expiryDate = model.getClock().now() + 3600; //expires one hour in future
        const char *idTag = "mIdTag";
        const char *parentIdTag = nullptr;

        rService->updateReservation(reservationId, connectorId, expiryDate, idTag, parentIdTag);
        
        REQUIRE( connector->getStatus() == ChargePointStatus::Reserved );

        //transaction blocked by reservation
        bool checkTxRejected = false;
        setTxNotificationOutput([&checkTxRejected] (Transaction*, TxNotification txNotification) {
            if (txNotification == TxNotification::ReservationConflict) {
                checkTxRejected = true;
            }
        });

        beginTransaction("wrong idTag");
        loop();
        REQUIRE( connector->getStatus() == ChargePointStatus::Reserved );
        REQUIRE( checkTxRejected );

        mocpp_deinitialize();
    }
}
