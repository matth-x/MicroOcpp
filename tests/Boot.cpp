// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Operations/BootNotification.h>
#include <MicroOcpp/Operations/StatusNotification.h>
#include <MicroOcpp/Operations/CustomOperation.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Debug.h>
#include <catch2/catch.hpp>
#include "./helpers/testHelper.h"

#define CHARGEPOINTMODEL "Test model"
#define CHARGEPOINTVENDOR "Test vendor"

#define BASE_TIME "2023-01-01T00:00:00.000Z"

#define GET_CONFIGURATION "[2,\"msgId01\",\"GetConfiguration\",{\"key\":[]}]"
#define TRIGGER_MESSAGE "[2,\"msgId02\",\"TriggerMessage\",{\"requestedMessage\":\"TriggeredOperation\"}]"

using namespace MicroOcpp;

//dummy operation type to test TriggerMessage
class TriggeredOperation : public Operation {
private:
    bool& checkExecuted;
public:
    TriggeredOperation(bool& checkExecuted) : checkExecuted(checkExecuted) { }
    const char* getOperationType() override {return "TriggeredOperation";}
    std::unique_ptr<JsonDoc> createReq() override {
        checkExecuted = true;
        return createEmptyDocument();
    }
    void processConf(JsonObject) override {}
    void processReq(JsonObject) override {}
    std::unique_ptr<JsonDoc> createConf() override {return createEmptyDocument();}
};


TEST_CASE( "Boot Behavior" ) {
    printf("\nRun %s\n",  "Boot Behavior");

    //clean state
    auto filesystem = makeDefaultFilesystemAdapter(FilesystemOpt::Use_Mount_FormatOnFail);
    FilesystemUtils::remove_if(filesystem, [] (const char*) {return true;});

    //initialize Context with dummy socket
    LoopbackConnection loopback;


    mocpp_initialize(loopback, ChargerCredentials(CHARGEPOINTMODEL, CHARGEPOINTVENDOR), filesystem);

    mocpp_set_timer(custom_timer_cb);

    SECTION("BootNotification - Accepted") {

        bool checkProcessed = false;

        getOcppContext()->getOperationRegistry().registerOperation("BootNotification",
            [&checkProcessed] () {
                return new Ocpp16::CustomOperation("BootNotification",
                    [ &checkProcessed] (JsonObject payload) {
                        //process req
                        checkProcessed = true;
                        REQUIRE( !strcmp(payload["chargePointModel"] | "_Undefined", CHARGEPOINTMODEL) );
                        REQUIRE( !strcmp(payload["chargePointVendor"] | "_Undefined", CHARGEPOINTVENDOR) );
                    },
                    [] () {
                        //create conf
                        auto conf = makeJsonDoc(UNIT_MEM_TAG, 1024);
                        (*conf)["currentTime"] = BASE_TIME;
                        (*conf)["interval"] = 3600;
                        (*conf)["status"] = "Accepted";
                        return conf;
                    });
            });
        
        loop();

        REQUIRE(checkProcessed);
        REQUIRE(getOcppContext()->getModel().getClock().now() >= MIN_TIME);
    }

    SECTION("BootNotification - Pending") {

        MO_DBG_INFO("Queue messages before BootNotification to see if they come through");

        loop(); //normal BootNotification run

        REQUIRE( isOperative() ); //normal BN succeeded

        loopback.setOnline( false );

        beginTransaction_authorized("mIdTag");

        loop();

        endTransaction();

        mocpp_deinitialize();

        loopback.setOnline( true );

        MO_DBG_INFO("Start charger again with queued transaction messages, also init non-tx-related msg, but now delay BN procedure");

        mocpp_initialize(loopback, ChargerCredentials());

        getOcppContext()->getOperationRegistry().registerOperation("BootNotification",
            [] () {
                return new Ocpp16::CustomOperation("BootNotification",
                    [] (JsonObject payload) {
                        //ignore req
                    },
                    [] () {
                        //create conf
                        auto conf = makeJsonDoc(UNIT_MEM_TAG, 1024);
                        (*conf)["currentTime"] = BASE_TIME;
                        (*conf)["interval"] = 3600;
                        (*conf)["status"] = "Pending";
                        return conf;
                    });
            });

        bool sentTxMsg = false;

        getOcppContext()->getOperationRegistry().setOnRequest("StartTransaction",
            [&sentTxMsg] (JsonObject) {
                sentTxMsg = true;
            });

        getOcppContext()->getOperationRegistry().setOnRequest("StopTransaction",
            [&sentTxMsg] (JsonObject) {
                sentTxMsg = true;
            });
        
        bool checkProcessedHeartbeat = false;

        auto heartbeat = makeRequest(new Ocpp16::CustomOperation(
                "Heartbeat",
                [] () {
                    //create req
                    return createEmptyDocument();},
                [&checkProcessedHeartbeat] (JsonObject) {
                    //process conf
                    checkProcessedHeartbeat = true;
                }));
        heartbeat->setTimeout(0); //disable timeout and check if message will be sent later
        
        getOcppContext()->initiateRequest(std::move(heartbeat));

        bool sentNonTxMsg = false;

        getOcppContext()->getOperationRegistry().setOnRequest("Heartbeat",
            [&sentNonTxMsg] (JsonObject) {
                sentNonTxMsg = true;
            });

        loop();

        REQUIRE( !sentTxMsg );
        REQUIRE( !sentNonTxMsg );
        REQUIRE( !checkProcessedHeartbeat );

        MO_DBG_INFO("Check if charger still responds to server-side messages and executes TriggerMessages");

        bool reactedToServerMsg = false;

        getOcppContext()->getOperationRegistry().setOnRequest("GetConfiguration",
            [&reactedToServerMsg] (JsonObject) {
                reactedToServerMsg = true;
            });

        loopback.sendTXT(GET_CONFIGURATION, sizeof(GET_CONFIGURATION) - 1);

        loop();

        REQUIRE( reactedToServerMsg );

        bool executedTriggerMessage = false;

        getOcppContext()->getOperationRegistry().registerOperation("TriggeredOperation",
            [&executedTriggerMessage] () {return new TriggeredOperation(executedTriggerMessage);});
        
        loopback.sendTXT(TRIGGER_MESSAGE, sizeof(TRIGGER_MESSAGE) - 1);

        loop();

        REQUIRE( executedTriggerMessage );

        //other messages still didn't get through?
        REQUIRE( !sentTxMsg );
        REQUIRE( !sentNonTxMsg );
        REQUIRE( !checkProcessedHeartbeat );

        MO_DBG_INFO("Now, accept BN and check if all queued messages finally arrive");

        getOcppContext()->getOperationRegistry().registerOperation("BootNotification",
            [] () {
                return new Ocpp16::CustomOperation("BootNotification",
                    [] (JsonObject payload) {
                        //ignore req
                    },
                    [] () {
                        //create conf
                        auto conf = makeJsonDoc(UNIT_MEM_TAG, 1024);
                        (*conf)["currentTime"] = BASE_TIME;
                        (*conf)["interval"] = 3600;
                        (*conf)["status"] = "Accepted";
                        return conf;
                    });
            });

        mtime += 3600 * 1000;

        loop();

        REQUIRE( sentTxMsg );
        REQUIRE( sentNonTxMsg );
        REQUIRE( checkProcessedHeartbeat );
    }

    SECTION("PreBoot transactions") {
        declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "PreBootTransactions", true)->setBool(true);
        declareConfiguration<bool>("AllowOfflineTxForUnknownId", true)->setBool(true);

        unsigned int startTxCount = 0;

        getOcppContext()->getOperationRegistry().setOnRequest("StartTransaction",
            [&startTxCount] (JsonObject) {
                startTxCount++;
            });

        //start one transaction in full offline mode

        loopback.setOnline( false );
        loop();
        REQUIRE( getChargePointStatus() == ChargePointStatus_Available );

        beginTransaction_authorized("mIdTag");
        loop();
        REQUIRE( getChargePointStatus() == ChargePointStatus_Charging ); 

        endTransaction("mIdTag");
        loop();
        REQUIRE( getChargePointStatus() == ChargePointStatus_Available );

        //start another transaction while BN is pending

        getOcppContext()->getOperationRegistry().registerOperation("BootNotification",
            [] () {
                return new Ocpp16::CustomOperation("BootNotification",
                    [] (JsonObject payload) {
                        //ignore req
                    },
                    [] () {
                        //create conf
                        auto conf = makeJsonDoc(UNIT_MEM_TAG, 1024);
                        (*conf)["currentTime"] = BASE_TIME;
                        (*conf)["interval"] = 3600;
                        (*conf)["status"] = "Pending";
                        return conf;
                    });
            });

        loopback.setOnline( true );
        loop();
        REQUIRE( startTxCount == 0 );

        beginTransaction_authorized("mIdTag");
        loop();
        REQUIRE( getChargePointStatus() == ChargePointStatus_Charging ); 

        endTransaction("mIdTag");
        loop();
        REQUIRE( getChargePointStatus() == ChargePointStatus_Available );

        REQUIRE( startTxCount == 0 );

        //Now, accept BN and check again

        getOcppContext()->getOperationRegistry().registerOperation("BootNotification",
            [] () {
                return new Ocpp16::CustomOperation("BootNotification",
                    [] (JsonObject payload) {
                        //ignore req
                    },
                    [] () {
                        //create conf
                        auto conf = makeJsonDoc(UNIT_MEM_TAG, 1024);
                        (*conf)["currentTime"] = BASE_TIME;
                        (*conf)["interval"] = 3600;
                        (*conf)["status"] = "Accepted";
                        return conf;
                    });
            });

        mtime += 3600 * 1000;

        loop();
        REQUIRE( startTxCount == 2 );

    }

    mocpp_deinitialize();
}
