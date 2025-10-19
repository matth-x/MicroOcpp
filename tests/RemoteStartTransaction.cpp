// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/OperationRegistry.h>
#include <MicroOcpp/Operations/CustomOperation.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/ConnectorBase/Connector.h>
#include <MicroOcpp/Operations/RemoteStartTransaction.h>
#include <MicroOcpp/Debug.h>
#include <catch2/catch.hpp>
#include "./helpers/testHelper.h"

using namespace MicroOcpp;

TEST_CASE("RemoteStartTransaction") {
    printf("\nRun %s\n", "RemoteStartTransaction");

    LoopbackConnection loopback;
    mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
    mocpp_set_timer(custom_timer_cb);
    loop();

    auto context = getOcppContext();
    auto connector = context->getModel().getConnector(1);

    SECTION("Basic remote start accepted") {
        // Ensure connector idle
        REQUIRE(connector->getStatus() == ChargePointStatus_Available);

        context->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
            "RemoteStartTransaction",
            [] () {
                auto doc = makeJsonDoc(UNIT_MEM_TAG, JSON_OBJECT_SIZE(2));
                auto payload = doc->to<JsonObject>();
                payload["idTag"] = "mIdTag";
                return doc;},
            [] (JsonObject) {}
        )));

        loop();
        REQUIRE(connector->getStatus() == ChargePointStatus_Charging);
        endTransaction();
        loop();
        REQUIRE(connector->getStatus() == ChargePointStatus_Available);
    }

    SECTION("Same connectorId rejected when transaction active") {
        // Start with connector 1 busy so remote start with connectorId=1 should not auto-assign
        beginTransaction("anotherId");
        loop();
        REQUIRE(connector->getStatus() == ChargePointStatus_Charging);

        bool checkProcessed = false;

        context->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
            "RemoteStartTransaction",
            [] () {
                auto doc = makeJsonDoc(UNIT_MEM_TAG, JSON_OBJECT_SIZE(3));
                auto payload = doc->to<JsonObject>();
                payload["idTag"] = "mIdTag";
                payload["connectorId"] = 1; // the same connector already in use
                return doc;},
            [&checkProcessed] (JsonObject response) {
                checkProcessed = true;
                REQUIRE( !strcmp(response["status"] | "_Undefined", "Rejected") );
            }
        )));

        loop();

        // Transaction should still be the original one only
        REQUIRE(checkProcessed);
        REQUIRE(connector->getTransaction());
        REQUIRE(strcmp(connector->getTransaction()->getIdTag(), "anotherId") == 0);
        REQUIRE(connector->getStatus() == ChargePointStatus_Charging);

        endTransaction();
        loop();
        REQUIRE(connector->getStatus() == ChargePointStatus_Available);
    }

    SECTION("ConnectorId 0 rejected per spec") {
        // RemoteStartTransaction response status is Rejected when connectorId == 0
        REQUIRE(connector->getStatus() == ChargePointStatus_Available);

        bool checkProcessed = false;

        context->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
            "RemoteStartTransaction",
            [] () {
                auto doc = makeJsonDoc(UNIT_MEM_TAG, JSON_OBJECT_SIZE(3));
                auto payload = doc->to<JsonObject>();
                payload["idTag"] = "mIdTag";
                payload["connectorId"] = 0; // invalid per spec
                return doc;},
            [&checkProcessed] (JsonObject response) {
                checkProcessed = true;
                REQUIRE( !strcmp(response["status"] | "_Undefined", "Rejected") );
            }
        )));

        loop();

        REQUIRE(checkProcessed);
        REQUIRE(connector->getStatus() == ChargePointStatus_Available);
    }

    SECTION("No free connector so rejected") {
        // Occupy all connectors (limit defined by MO_NUMCONNECTORS)
        for (unsigned cId = 1; cId < context->getModel().getNumConnectors(); cId++) {
            auto c = context->getModel().getConnector(cId);
            if (c) {
                c->beginTransaction_authorized("busyId");
            }
        }
        loop();

        bool checkProcessed = false;
        auto freeFound = false;
        for (unsigned cId = 1; cId < context->getModel().getNumConnectors(); cId++) {
            auto c = context->getModel().getConnector(cId);
            if (c && !c->getTransaction()) freeFound = true;
        }
        REQUIRE(!freeFound); // ensure all are busy

        context->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
            "RemoteStartTransaction",
            [] () {
                auto doc = makeJsonDoc(UNIT_MEM_TAG, JSON_OBJECT_SIZE(2));
                auto payload = doc->to<JsonObject>();
                payload["idTag"] = "mIdTag";
                return doc;},
            [&checkProcessed] (JsonObject response) {
                checkProcessed = true;
                REQUIRE( !strcmp(response["status"] | "_Undefined", "Rejected") );
            }
        )));

        loop();
        REQUIRE(checkProcessed);

        // No new transaction should be created; keep statuses
        int activeTx = 0;
        for (unsigned cId = 1; cId < context->getModel().getNumConnectors(); cId++) {
            auto c = context->getModel().getConnector(cId);
            if (c && c->getTransaction()) activeTx++;
        }
        REQUIRE(activeTx == (int)context->getModel().getNumConnectors() - 1); // all occupied

        // cleanup
        for (unsigned cId = 1; cId < context->getModel().getNumConnectors(); cId++) {
            auto c = context->getModel().getConnector(cId);
            if (c && c->getTransaction()) {
                c->endTransaction();
            }
        }
        loop();
        REQUIRE(connector->getStatus() == ChargePointStatus_Available);
    }

    mocpp_deinitialize();
}
