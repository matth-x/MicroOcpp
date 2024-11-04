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
#include <MicroOcpp/Model/Boot/BootService.h>
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

        loopback.setConnected( false );

        beginTransaction_authorized("mIdTag");

        loop();

        endTransaction();

        mocpp_deinitialize();

        loopback.setConnected( true );

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

        loopback.setConnected( false );
        loop();
        REQUIRE( getChargePointStatus() == ChargePointStatus_Available );

        beginTransaction("mIdTag");
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

        loopback.setConnected( true );
        loop();
        REQUIRE( startTxCount == 0 );

        beginTransaction("mIdTag2");
        mtime += 20 * 1000;
        loop();
        REQUIRE( getChargePointStatus() == ChargePointStatus_Charging ); 

        endTransaction();
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

    SECTION("Auto recovery") {

        //start transaction which will persist a few boot cycles, but then will be wiped by auto recovery
        loop();
        beginTransaction("mIdTag");
        loop();
        REQUIRE( getChargePointStatus() == ChargePointStatus_Charging );

        declareConfiguration<const char*>("keepConfigOverRecovery", "originalVal");
        configuration_save();

        mocpp_deinitialize();

        //MO has 2 unexpected power cycles. Probably just back luck - keep the local state and configuration

        //Increase the power cycle counter manually because it's not possible to interrupt the MO lifecycle during unit tests
        BootStats bootstats;
        BootService::loadBootStats(filesystem, bootstats);
        bootstats.bootNr += 2;
        BootService::storeBootStats(filesystem, bootstats);

        mocpp_initialize(loopback, ChargerCredentials(), filesystem, /*enable auto recovery*/ true);
        BootService::loadBootStats(filesystem, bootstats);
        REQUIRE( bootstats.getBootFailureCount() == 2 + 1 ); //two boot failures have been measured, +1 because each power cycle is counted as potentially failing until reaching the long runtime barrier

        loop();

        REQUIRE( getChargePointStatus() == ChargePointStatus_Charging );

        REQUIRE( !strcmp(declareConfiguration<const char*>("keepConfigOverRecovery", "otherVal")->getString(), "originalVal") );

        //check that the power cycle counter has been updated properly after the controller has been running stable over a long time
        mtime += MO_BOOTSTATS_LONGTIME_MS;
        loop();
        BootService::loadBootStats(filesystem, bootstats);
        REQUIRE( bootstats.getBootFailureCount() == 0 );

        mocpp_deinitialize();

        //MO has 10 power cycles without running for at least 3 minutes and wipes the local state, but keeps the configuration

        BootStats bootstats2;
        BootService::loadBootStats(filesystem, bootstats2);
        bootstats2.bootNr += 10;
        BootService::storeBootStats(filesystem, bootstats2);

        mocpp_initialize(loopback, ChargerCredentials(), filesystem, /*enable auto recovery*/ true);

        REQUIRE( !strcmp(declareConfiguration<const char*>("keepConfigOverRecovery", "otherVal")->getString(), "originalVal") );
        BootStats bootstats3;
        BootService::loadBootStats(filesystem, bootstats3);
        REQUIRE( bootstats3.getBootFailureCount() == 0 + 1 ); //failure count is reset, but +1 because each power cycle is counted as potentially failing until reaching the long runtime barrier

        loop();
        REQUIRE( getChargePointStatus() == ChargePointStatus_Available );

    }

    SECTION("Migration") {

        //migration removes files from previous MO versions which were running on the controller. This includes the
        //transaction cache, but configs are preserved

        auto old_opstore = filesystem->open(MO_FILENAME_PREFIX "opstore.jsn", "w"); //the opstore has been removed in MO v1.2.0
        old_opstore->write("example content", sizeof("example content") - 1);
        old_opstore.reset(); //flushes the file

        loop();
        beginTransaction("mIdTag"); //tx store will also be removed
        auto tx = getTransaction();
        auto txNr = tx->getTxNr(); //remember this for later usage
        tx.reset(); //reset this smart pointer
        loop();
        REQUIRE( getChargePointStatus() == ChargePointStatus_Charging );
        endTransaction();
        loop();

        REQUIRE( getOcppContext()->getModel().getTransactionStore()->getTransaction(1, txNr) != nullptr ); //tx exists on flash

        declareConfiguration<const char*>("keepConfigOverMigration", "originalVal"); //migration keeps configs
        configuration_save();

        mocpp_deinitialize();

        //After a FW update, the tracked version number has changed
        BootStats bootstats;
        BootService::loadBootStats(filesystem, bootstats);
        snprintf(bootstats.microOcppVersion, sizeof(bootstats.microOcppVersion), "oldFwVers");
        BootService::storeBootStats(filesystem, bootstats);

        mocpp_initialize(loopback, ChargerCredentials(), filesystem); //MO migrates here

        size_t msize = 0;
        REQUIRE( filesystem->stat(MO_FILENAME_PREFIX "opstore.jsn", &msize) != 0 ); //opstore has been removed

        REQUIRE( getOcppContext()->getModel().getTransactionStore()->getTransaction(1, txNr) == nullptr ); //tx history entry has been removed

        REQUIRE( !strcmp(declareConfiguration<const char*>("keepConfigOverMigration", "otherVal")->getString(), "originalVal") ); //config has been preserved
    }

    SECTION("Clean unused configs") {

        declareConfiguration<const char*>("neverDeclaredInsideMO", "originalVal"); //unused configs will be cleared automatically after the controller has been running for a long time
        configuration_save();

        mocpp_deinitialize();

        mocpp_initialize(loopback, ChargerCredentials(), filesystem); //all configs are loaded here, including the test config of this section
        loop();

        //unused configs will be cleared automatically after long time
        mtime += MO_BOOTSTATS_LONGTIME_MS;
        loop();

        REQUIRE( !strcmp(declareConfiguration<const char*>("neverDeclaredInsideMO", "newVal")->getString(), "newVal") ); //config has been removed
    }

    SECTION("Boot with v201") {

        mocpp_deinitialize();

        mocpp_initialize(loopback, ChargerCredentials::v201(CHARGEPOINTMODEL, CHARGEPOINTVENDOR), filesystem, false, ProtocolVersion(2,0,1));

        bool checkProcessed = false;

        getOcppContext()->getOperationRegistry().registerOperation("BootNotification",
            [&checkProcessed] () {
                return new Ocpp16::CustomOperation("BootNotification",
                    [ &checkProcessed] (JsonObject payload) {
                        //process req
                        checkProcessed = true;
                        REQUIRE( !strcmp(payload["reason"] | "_Undefined", "PowerUp") );
                        REQUIRE( !strcmp(payload["chargingStation"]["model"] | "_Undefined", CHARGEPOINTMODEL) );
                        REQUIRE( !strcmp(payload["chargingStation"]["vendorName"] | "_Undefined", CHARGEPOINTVENDOR) );
                    },
                    [] () {
                        //create conf
                        auto conf = makeJsonDoc(UNIT_MEM_TAG, JSON_OBJECT_SIZE(3));
                        (*conf)["currentTime"] = BASE_TIME;
                        (*conf)["interval"] = 3600;
                        (*conf)["status"] = "Accepted";
                        return conf;
                    });
            });

        MO_MEM_RESET();
        
        loop();

        REQUIRE(checkProcessed);
        REQUIRE(getOcppContext()->getModel().getClock().now() >= MIN_TIME);

        MO_MEM_PRINT_STATS();

        MO_MEM_RESET();

        mtime += 3600 * 1000;
        loop();

        MO_DBG_INFO("Memory requirements UC G02:");
        MO_MEM_PRINT_STATS();
    }

    mocpp_deinitialize();
}
