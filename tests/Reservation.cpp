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

    mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
    auto& model = getOcppContext()->getModel();
    auto rService = model.getReservationService();
    auto connector = model.getConnector(1);
    model.getClock().setTime(BASE_TIME);

    loop();

    SECTION("Basic reservation") {
        REQUIRE( connector->getStatus() == ChargePointStatus::Available );
        REQUIRE( rService );

        //set reservation
        int reservationId = 123;
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

        //idTag matches reservation
        beginTransaction("mIdTag");
        loop();
        REQUIRE( connector->getStatus() == ChargePointStatus::Charging );
        REQUIRE( connector->getTransaction()->getReservationId() == reservationId );

        //reservation is reset after tx
        endTransaction();
        loop();
        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        //RemoteStartTx - idTag doesn't match. The tx will start anyway assuming some start trigger in the backend prevails over reservations in the backend implementation
        rService->updateReservation(reservationId, connectorId, expiryDate, idTag, parentIdTag);
        REQUIRE( connector->getStatus() == ChargePointStatus::Reserved );

        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "RemoteStartTransaction",
                [] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
                    auto payload = doc->to<JsonObject>();
                    payload["idTag"] = "wrong idTag";
                    return doc;},
                [] (JsonObject) { } //ignore conf
        )));
        loop();
        REQUIRE( connector->getStatus() == ChargePointStatus::Charging );
        REQUIRE( connector->getTransaction()->getReservationId() != reservationId );

        //reservation is reset after tx
        endTransaction();
        loop();
        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        //RemoteStartTx - idTag does match
        rService->updateReservation(reservationId, connectorId, expiryDate, idTag, parentIdTag);
        REQUIRE( connector->getStatus() == ChargePointStatus::Reserved );

        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "RemoteStartTransaction",
                [idTag] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
                    auto payload = doc->to<JsonObject>();
                    payload["idTag"] = idTag;
                    return doc;},
                [] (JsonObject) { } //ignore conf
        )));
        loop();
        REQUIRE( connector->getStatus() == ChargePointStatus::Charging );
        REQUIRE( connector->getTransaction()->getReservationId() == reservationId );

        //reservation is reset after tx
        endTransaction();
        loop();
        REQUIRE( connector->getStatus() == ChargePointStatus::Available );
    }

    SECTION("Tx on other connector") {
        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        //set reservation
        int reservationId = 123;
        unsigned int connectorIdResvd = 1; //reserve connector 1
        unsigned int connectorIdOther = 2; //start charging on other connector
        Timestamp expiryDate = model.getClock().now() + 3600; //expires one hour in future
        const char *idTag = "mIdTag";
        const char *parentIdTag = nullptr;

        rService->updateReservation(reservationId, connectorIdResvd, expiryDate, idTag, parentIdTag);
        REQUIRE( model.getConnector(connectorIdResvd)->getStatus() == ChargePointStatus::Reserved );

        beginTransaction(idTag, connectorIdOther);
        loop();
        REQUIRE( model.getConnector(connectorIdResvd)->getStatus() == ChargePointStatus::Available ); //reservation on first connector withdrawed
        REQUIRE( model.getConnector(connectorIdOther)->getStatus() == ChargePointStatus::Charging );
        REQUIRE( getTransaction(connectorIdOther)->getReservationId() == reservationId ); //reservation transferred to other connector

        endTransaction(nullptr, nullptr, connectorIdOther);
        loop();
        REQUIRE( model.getConnector(connectorIdOther)->getStatus() == ChargePointStatus::Available );
    }

    SECTION("parentIdTag") {
        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        //set reservation
        int reservationId = 123;
        unsigned int connectorId = 1;
        Timestamp expiryDate = model.getClock().now() + 3600; //expires one hour in future
        const char *idTag = "mIdTag";
        const char *parentIdTag = "mParentIdTag";
        
        rService->updateReservation(reservationId, connectorId, expiryDate, idTag, parentIdTag);
        REQUIRE( connector->getStatus() == ChargePointStatus::Reserved );

        bool checkProcessed = false;
        getOcppContext()->getOperationRegistry().registerOperation("Authorize",
            [parentIdTag, &checkProcessed] () {
                return new Ocpp16::CustomOperation("Authorize",
                    [] (JsonObject) {}, //ignore req payload
                    [parentIdTag, &checkProcessed] () {
                        //create conf
                        checkProcessed = true;
                        auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
                                JSON_OBJECT_SIZE(1) + //payload root
                                JSON_OBJECT_SIZE(3))); //idTagInfo
                        auto payload = doc->to<JsonObject>();
                        payload["idTagInfo"]["parentIdTag"] = parentIdTag;
                        payload["idTagInfo"]["status"] = "Accepted";
                        return doc;});
            });
        beginTransaction("other idTag");
        loop();
        REQUIRE( checkProcessed );
        REQUIRE( connector->getStatus() == ChargePointStatus::Charging );
        REQUIRE( connector->getTransaction()->getReservationId() == reservationId );

        //reset tx
        endTransaction();
        loop();
        REQUIRE( connector->getStatus() == ChargePointStatus::Available );
    }

    SECTION("ConnectorZero") {
        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        //set reservation
        Timestamp expiryDate = model.getClock().now() + 3600; //expires one hour in future
        const char *idTag = "mIdTag";
        const char *parentIdTag = nullptr;

        //if connector 0 is reserved, accept at most one further reservation
        REQUIRE( rService->updateReservation(1000, 0, expiryDate, idTag, parentIdTag) );
        REQUIRE( rService->updateReservation(1001, 1, expiryDate, idTag, parentIdTag) );
        REQUIRE( !rService->updateReservation(1002, 2, expiryDate, idTag, parentIdTag) );
        REQUIRE( model.getConnector(2)->getStatus() == ChargePointStatus::Available );

        //reset reservations
        rService->getReservationById(1000)->clear();
        rService->getReservationById(1001)->clear();
        REQUIRE( model.getConnector(1)->getStatus() == ChargePointStatus::Available );

        //if connector 0 is reserved, ensure that at least one physical connector remains available for the idTag of the reservation
        REQUIRE( rService->updateReservation(1000, 0, expiryDate, idTag, parentIdTag) );

        beginTransaction("other idTag", 1);
        loop();
        REQUIRE( model.getConnector(1)->getStatus() == ChargePointStatus::Charging );

        bool checkTxRejected = false;
        setTxNotificationOutput([&checkTxRejected] (Transaction*, TxNotification txNotification) {
            if (txNotification == TxNotification::ReservationConflict) {
                checkTxRejected = true;
            }
        }, 2);

        beginTransaction("other idTag 2", 2);
        loop();
        REQUIRE( checkTxRejected );
        REQUIRE( model.getConnector(2)->getStatus() == ChargePointStatus::Available );
        

        endTransaction(nullptr, nullptr, 1);
        loop();
    }

    SECTION("Expiry date") {
        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        //set reservation
        int reservationId = 123;
        unsigned int connectorId = 1;
        Timestamp expiryDate = model.getClock().now() + 3600; //expires one hour in future
        const char *idTag = "mIdTag";
        const char *parentIdTag = nullptr;

        rService->updateReservation(reservationId, connectorId, expiryDate, idTag, parentIdTag);
        
        REQUIRE( connector->getStatus() == ChargePointStatus::Reserved );

        Timestamp expired = expiryDate + 1;
        char expired_cstr [JSONDATE_LENGTH + 1];
        expired.toJsonString(expired_cstr, JSONDATE_LENGTH + 1);
        model.getClock().setTime(expired_cstr);

        REQUIRE( connector->getStatus() == ChargePointStatus::Available );
    }

    SECTION("Reservation persistency") {
        unsigned int connectorId = 1;
        REQUIRE( getOcppContext()->getModel().getConnector(connectorId)->getStatus() == ChargePointStatus::Available );

        //set reservation
        int reservationId = 123;
        Timestamp expiryDate = model.getClock().now() + 3600; //expires one hour in future
        const char *idTag = "mIdTag";
        const char *parentIdTag = "mParentIdTag";

        getOcppContext()->getModel().getReservationService()->updateReservation(reservationId, connectorId, expiryDate, idTag, parentIdTag);
        
        REQUIRE( getOcppContext()->getModel().getConnector(connectorId)->getStatus() == ChargePointStatus::Reserved );

        mocpp_deinitialize();

        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        getOcppContext()->getModel().getClock().setTime(BASE_TIME);
        loop();

        REQUIRE( getOcppContext()->getModel().getConnector(connectorId)->getStatus() == ChargePointStatus::Reserved );

        auto reservation = getOcppContext()->getModel().getReservationService()->getReservationById(reservationId);
        REQUIRE( reservation->getReservationId() == reservationId );
        REQUIRE( reservation->getConnectorId() == connectorId );
        REQUIRE( reservation->getExpiryDate() == expiryDate );
        REQUIRE( !strcmp(reservation->getIdTag(), idTag) );
        REQUIRE( !strcmp(reservation->getParentIdTag(), parentIdTag) );

        reservation->clear();

        mocpp_deinitialize();

        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        getOcppContext()->getModel().getClock().setTime(BASE_TIME);
        loop();

        REQUIRE( getOcppContext()->getModel().getConnector(connectorId)->getStatus() == ChargePointStatus::Available );
    }

    SECTION("ReserveNow") {

        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        //set reservation
        int reservationId = 123;
        unsigned int connectorId = 1;
        Timestamp expiryDate = model.getClock().now() + 3600; //expires one hour in future
        const char *idTag = "mIdTag";
        const char *parentIdTag = nullptr;

        //simple reservation
        bool checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "ReserveNow",
                [reservationId, connectorId, expiryDate, idTag, parentIdTag] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
                            JSON_OBJECT_SIZE(5) + 
                            JSONDATE_LENGTH + 1));
                    auto payload = doc->to<JsonObject>();
                    payload["connectorId"] = connectorId;
                    char expiryDate_cstr [JSONDATE_LENGTH + 1];
                    expiryDate.toJsonString(expiryDate_cstr, JSONDATE_LENGTH + 1);
                    payload["expiryDate"] = expiryDate_cstr;
                    payload["idTag"] = idTag;
                    payload["parentIdTag"] = parentIdTag;
                    payload["reservationId"] = reservationId;
                    return doc;},
                [&checkProcessed] (JsonObject payload) {
                    //receive conf
                    checkProcessed = true;

                    REQUIRE( !strcmp(payload["status"] | "_Undefined", "Accepted") );
                }
        )));
        loop();
        REQUIRE( checkProcessed );
        REQUIRE( connector->getStatus() == ChargePointStatus::Reserved );

        model.getReservationService()->getReservationById(reservationId)->clear();
        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        //reserve while charger is in Faulted state
        const char *errorCode = "OtherError";
        addErrorCodeInput([&errorCode] () {return errorCode;});
        REQUIRE( connector->getStatus() == ChargePointStatus::Faulted );

        checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "ReserveNow",
                [reservationId, connectorId, expiryDate, idTag, parentIdTag] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
                            JSON_OBJECT_SIZE(5) + 
                            JSONDATE_LENGTH + 1));
                    auto payload = doc->to<JsonObject>();
                    payload["connectorId"] = connectorId;
                    char expiryDate_cstr [JSONDATE_LENGTH + 1];
                    expiryDate.toJsonString(expiryDate_cstr, JSONDATE_LENGTH + 1);
                    payload["expiryDate"] = expiryDate_cstr;
                    payload["idTag"] = idTag;
                    payload["parentIdTag"] = parentIdTag;
                    payload["reservationId"] = reservationId;
                    return doc;},
                [&checkProcessed] (JsonObject payload) {
                    //receive conf
                    checkProcessed = true;

                    REQUIRE( !strcmp(payload["status"] | "_Undefined", "Faulted") );
                }
        )));
        loop();
        REQUIRE( checkProcessed );
        REQUIRE( connector->getStatus() == ChargePointStatus::Faulted );

        errorCode = nullptr; //reset error
        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        //reserve while connector is already occupied
        setConnectorPluggedInput([] {return true;}); //plug EV
        REQUIRE( connector->getStatus() == ChargePointStatus::Preparing );

        checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "ReserveNow",
                [reservationId, connectorId, expiryDate, idTag, parentIdTag] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
                            JSON_OBJECT_SIZE(5) + 
                            JSONDATE_LENGTH + 1));
                    auto payload = doc->to<JsonObject>();
                    payload["connectorId"] = connectorId;
                    char expiryDate_cstr [JSONDATE_LENGTH + 1];
                    expiryDate.toJsonString(expiryDate_cstr, JSONDATE_LENGTH + 1);
                    payload["expiryDate"] = expiryDate_cstr;
                    payload["idTag"] = idTag;
                    payload["parentIdTag"] = parentIdTag;
                    payload["reservationId"] = reservationId;
                    return doc;},
                [&checkProcessed] (JsonObject payload) {
                    //receive conf
                    checkProcessed = true;

                    REQUIRE( !strcmp(payload["status"] | "_Undefined", "Occupied") );
                }
        )));
        loop();
        REQUIRE( checkProcessed );
        REQUIRE( connector->getStatus() == ChargePointStatus::Preparing );

        setConnectorPluggedInput(nullptr); //reset ConnectorPluggedInput
        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        //Rejected ReserveNow status not possible

        //reserve while connector is inoperative
        connector->setAvailabilityVolatile(false);
        REQUIRE( connector->getStatus() == ChargePointStatus::Unavailable );

        checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "ReserveNow",
                [reservationId, connectorId, expiryDate, idTag, parentIdTag] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
                            JSON_OBJECT_SIZE(5) + 
                            JSONDATE_LENGTH + 1));
                    auto payload = doc->to<JsonObject>();
                    payload["connectorId"] = connectorId;
                    char expiryDate_cstr [JSONDATE_LENGTH + 1];
                    expiryDate.toJsonString(expiryDate_cstr, JSONDATE_LENGTH + 1);
                    payload["expiryDate"] = expiryDate_cstr;
                    payload["idTag"] = idTag;
                    payload["parentIdTag"] = parentIdTag;
                    payload["reservationId"] = reservationId;
                    return doc;},
                [&checkProcessed] (JsonObject payload) {
                    //receive conf
                    checkProcessed = true;

                    REQUIRE( !strcmp(payload["status"] | "_Undefined", "Unavailable") );
                }
        )));
        loop();
        REQUIRE( checkProcessed );
        REQUIRE( connector->getStatus() == ChargePointStatus::Unavailable );

        connector->setAvailabilityVolatile(true); //revert Unavailable status
        REQUIRE( connector->getStatus() == ChargePointStatus::Available );
    }

    SECTION("CancelReservation") {
        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        //set reservation
        int reservationId = 123;
        unsigned int connectorId = 1;
        Timestamp expiryDate = model.getClock().now() + 3600; //expires one hour in future
        const char *idTag = "mIdTag";
        const char *parentIdTag = nullptr;

        rService->updateReservation(reservationId, connectorId, expiryDate, idTag, parentIdTag);
        REQUIRE( connector->getStatus() == ChargePointStatus::Reserved );

        //CancelReservation successfully
        bool checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "CancelReservation",
                [reservationId, connectorId, expiryDate, idTag, parentIdTag] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
                    auto payload = doc->to<JsonObject>();
                    payload["reservationId"] = reservationId;
                    return doc;},
                [&checkProcessed] (JsonObject payload) {
                    //receive conf
                    checkProcessed = true;

                    REQUIRE( !strcmp(payload["status"] | "_Undefined", "Accepted") );
                }
        )));
        loop();
        REQUIRE( checkProcessed );
        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        //CancelReservation while no reservation exists
        checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "CancelReservation",
                [reservationId, connectorId, expiryDate, idTag, parentIdTag] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
                    auto payload = doc->to<JsonObject>();
                    payload["reservationId"] = reservationId;
                    return doc;},
                [&checkProcessed] (JsonObject payload) {
                    //receive conf
                    checkProcessed = true;

                    REQUIRE( !strcmp(payload["status"] | "_Undefined", "Rejected") );
                }
        )));
        loop();
        REQUIRE( checkProcessed );
        REQUIRE( connector->getStatus() == ChargePointStatus::Available );
    }

    mocpp_deinitialize();
}
