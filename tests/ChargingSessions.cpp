// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Operations/BootNotification.h>
#include <MicroOcpp/Operations/StatusNotification.h>
#include <MicroOcpp/Operations/CustomOperation.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Debug.h>
#include <catch2/catch.hpp>
#include "./helpers/testHelper.h"

#include <array>

#define BASE_TIME "2023-01-01T00:00:00.000Z"

using namespace MicroOcpp;


TEST_CASE( "Charging sessions" ) {
    printf("\nRun %s\n",  "Charging sessions");

    //initialize Context with dummy socket
    LoopbackConnection loopback;
    mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));

    auto engine = getOcppContext();
    auto& checkMsg = engine->getOperationRegistry();

    mocpp_set_timer(custom_timer_cb);

    auto connectionTimeOutInt = declareConfiguration<int>("ConnectionTimeOut", 30, CONFIGURATION_FN);
    connectionTimeOutInt->setInt(30);
    auto minimumStatusDurationInt = declareConfiguration<int>("MinimumStatusDuration", 0, CONFIGURATION_FN);
    minimumStatusDurationInt->setInt(0);
    
    std::array<const char*, 2> expectedSN {"Available", "Available"};
    std::array<bool, 2> checkedSN {false, false};
    checkMsg.registerOperation("StatusNotification", [] () -> Operation* {return new Ocpp16::StatusNotification(0, ChargePointStatus_UNDEFINED, MIN_TIME);});
    checkMsg.setOnRequest("StatusNotification",
        [&checkedSN, &expectedSN] (JsonObject request) {
            int connectorId = request["connectorId"] | -1;
            if (connectorId == 0 || connectorId == 1) { //only test single connector case here
                checkedSN[connectorId] = !strcmp(request["status"] | "Invalid", expectedSN[connectorId]);
            }
        });

    SECTION("Check idle state"){

        bool checkedBN = false;
        checkMsg.registerOperation("BootNotification", [engine] () -> Operation* {return new Ocpp16::BootNotification(engine->getModel(), makeJsonDoc("UnitTests"));});
        checkMsg.setOnRequest("BootNotification",
            [&checkedBN] (JsonObject request) {
                checkedBN = !strcmp(request["chargePointModel"] | "Invalid", "test-runner1234");
            });
        
        REQUIRE( !isOperative() ); //not operative before reaching loop stage

        loop();
        loop();
        REQUIRE( checkedBN );
        REQUIRE( checkedSN[0] );
        REQUIRE( checkedSN[1] );
        REQUIRE( isOperative() );
        REQUIRE( !getTransaction() );
        REQUIRE( !ocppPermitsCharge() );
    }

    loop();

    SECTION("StartTx") {
        SECTION("StartTx directly"){
            startTransaction("mIdTag");
            loop();
            REQUIRE(ocppPermitsCharge());
        }

        SECTION("StartTx via session management - plug in first") {
            expectedSN[1] = "Preparing";
            setConnectorPluggedInput([] () {return true;});
            loop();
            REQUIRE(checkedSN[1]);

            checkedSN[1] = false;
            expectedSN[1] = "Charging";
            beginTransaction("mIdTag");
            loop();
            REQUIRE(checkedSN[1]);
            REQUIRE(ocppPermitsCharge());
        }

        SECTION("StartTx via session management - authorization first") {

            expectedSN[1] = "Preparing";
            setConnectorPluggedInput([] () {return false;});
            beginTransaction("mIdTag");
            loop();
            REQUIRE(checkedSN[1]);

            checkedSN[1] = false;
            expectedSN[1] = "Charging";
            setConnectorPluggedInput([] () {return true;});
            loop();
            REQUIRE(checkedSN[1]);
            REQUIRE(ocppPermitsCharge());
        }

        SECTION("StartTx via session management - no plug") {
            expectedSN[1] = "Charging";
            beginTransaction("mIdTag");
            loop();
            REQUIRE(checkedSN[1]);
        }

        SECTION("StartTx via session management - ConnectionTimeOut") {
            expectedSN[1] = "Preparing";
            setConnectorPluggedInput([] () {return false;});
            beginTransaction("mIdTag");
            loop();
            REQUIRE(checkedSN[1]);

            checkedSN[1] = false;
            expectedSN[1] = "Available";
            mtime += connectionTimeOutInt->getInt() * 1000;
            loop();
            REQUIRE(checkedSN[1]);
        }

        loop();
        if (ocppPermitsCharge()) {
            stopTransaction();
        }
        loop();
    }

    SECTION("StopTx") {
        startTransaction("mIdTag");
        loop();
        expectedSN[1] = "Available";

        SECTION("directly") {
            stopTransaction();
            loop();
            REQUIRE(checkedSN[1]);
            REQUIRE(!ocppPermitsCharge());
        }

        SECTION("via session management - deauthorize") {
            endTransaction();
            loop();
            REQUIRE(checkedSN[1]);
            REQUIRE(!ocppPermitsCharge());
        }

        SECTION("via session management - deauthorize first") {
            expectedSN[1] = "Finishing";
            setConnectorPluggedInput([] () {return true;});
            endTransaction();
            loop();
            REQUIRE(checkedSN[1]);
            REQUIRE(!ocppPermitsCharge());

            checkedSN[1] = false;
            expectedSN[1] = "Available";
            setConnectorPluggedInput([] () {return false;});
            loop();
            REQUIRE(checkedSN[1]);
            REQUIRE(!ocppPermitsCharge());
        }

        SECTION("via session management - plug out") {
            setConnectorPluggedInput([] () {return false;});
            loop();
            REQUIRE(checkedSN[1]);
            REQUIRE(!ocppPermitsCharge());
        }

        if (ocppPermitsCharge()) {
            stopTransaction();
        }
        loop();
    }

    SECTION("Preboot transactions - tx before BootNotification") {
        mocpp_deinitialize();

        loopback.setOnline(false);
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));

        declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "PreBootTransactions", true, CONFIGURATION_FN)->setBool(true);
        configuration_save();

        loop();

        beginTransaction_authorized("mIdTag");

        loop();

        REQUIRE(isTransactionRunning());

        mtime += 3600 * 1000; //transaction duration ~1h

        endTransaction();

        loop();

        mtime += 3600 * 1000; //set base time one hour later

        bool checkStartProcessed = false;

        getOcppContext()->getModel().getClock().setTime(BASE_TIME);
        Timestamp basetime = Timestamp();
        basetime.setTime(BASE_TIME);

        getOcppContext()->getOperationRegistry().setOnRequest("StartTransaction", 
            [&checkStartProcessed, basetime] (JsonObject payload) {
                checkStartProcessed = true;
                Timestamp timestamp;
                timestamp.setTime(payload["timestamp"].as<const char*>());                

                auto adjustmentDelay = basetime - timestamp;
                REQUIRE((adjustmentDelay > 2 * 3600 - 10 && adjustmentDelay < 2 * 3600 + 10));
            });
        
        bool checkStopProcessed = false;

        getOcppContext()->getOperationRegistry().setOnRequest("StopTransaction", 
            [&checkStopProcessed, basetime] (JsonObject payload) {
                checkStopProcessed = true;
                Timestamp timestamp;
                timestamp.setTime(payload["timestamp"].as<const char*>());                

                auto adjustmentDelay = basetime - timestamp;
                REQUIRE((adjustmentDelay > 3600 - 10 && adjustmentDelay < 3600 + 10));
            });
        
        loopback.setOnline(true);
        loop();

        REQUIRE(checkStartProcessed);
        REQUIRE(checkStopProcessed);
    }

    SECTION("Preboot transactions - lose StartTx timestamp") {

        mocpp_deinitialize();

        loopback.setOnline(false);
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));

        declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "PreBootTransactions", true, CONFIGURATION_FN)->setBool(true);
        configuration_save();

        loop();

        beginTransaction_authorized("mIdTag");
        loop();

        REQUIRE(isTransactionRunning());

        mocpp_deinitialize();

        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));

        declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "PreBootTransactions", true, CONFIGURATION_FN)->setBool(true);
        configuration_save();

        bool checkProcessed = false;

        getOcppContext()->getOperationRegistry().setOnRequest("StartTransaction", [&checkProcessed] (JsonObject) {
            checkProcessed = true;
        });

        getOcppContext()->getOperationRegistry().setOnRequest("StopTransaction", [&checkProcessed] (JsonObject) {
            checkProcessed = true;
        });

        loopback.setOnline(true);

        loop();

        REQUIRE(!isTransactionRunning());
        REQUIRE(!checkProcessed);
    }

    SECTION("Preboot transactions - lose StopTx timestamp") {
        
        const char *starTxTimestampStr = "2023-02-01T00:00:00.000Z";
        getOcppContext()->getModel().getClock().setTime(starTxTimestampStr);

        beginTransaction_authorized("mIdTag");

        loop();

        REQUIRE(isTransactionRunning());

        mocpp_deinitialize();

        loopback.setOnline(false);
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));

        declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "PreBootTransactions", true, CONFIGURATION_FN)->setBool(true);
        configuration_save();

        loop();

        REQUIRE(isTransactionRunning());

        endTransaction();

        loop();

        REQUIRE(!isTransactionRunning());

        mocpp_deinitialize();

        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));

        declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "PreBootTransactions", true, CONFIGURATION_FN)->setBool(true);
        configuration_save();

        bool checkProcessed = false;

        getOcppContext()->getOperationRegistry().setOnRequest("StopTransaction",
            [&checkProcessed, starTxTimestampStr] (JsonObject payload) {
                checkProcessed = true;
                Timestamp timestamp;
                timestamp.setTime(payload["timestamp"].as<const char*>());

                Timestamp starTxTimestamp = Timestamp();
                starTxTimestamp.setTime(starTxTimestampStr);

                auto adjustmentDelay = timestamp - starTxTimestamp;
                REQUIRE(adjustmentDelay == 1);
            });

        loopback.setOnline(true);

        loop();

        REQUIRE(checkProcessed);
    }

    SECTION("Preboot transactions - reject tx if limit exceeded") {
        mocpp_deinitialize();

        loopback.setConnected(false);
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));

        declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "PreBootTransactions", true, CONFIGURATION_FN)->setBool(true);
        declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "SilentOfflineTransactions", false, CONFIGURATION_FN)->setBool(false); // do not start more txs if tx journal is full
        configuration_save();

        loop();

        for (size_t i = 0; i < MO_TXRECORD_SIZE; i++) {
            beginTransaction_authorized("mIdTag");

            loop();

            REQUIRE(isTransactionRunning());

            endTransaction();

            loop();

            REQUIRE(!isTransactionRunning());
        }

        // now, tx journal is full. Block any further charging session

        auto tx_success = beginTransaction_authorized("mIdTag");
        REQUIRE( !tx_success );

        loop();

        REQUIRE(!isTransactionRunning());
        REQUIRE(!ocppPermitsCharge());

        // Check if all 4 cached transctions are transmitted after going online

        const int txId_base = 10000;
        int txId_generate = txId_base;
        int txId_confirm = txId_base;

        getOcppContext()->getOperationRegistry().registerOperation("StartTransaction", [&txId_generate] () {
            return new Ocpp16::CustomOperation("StartTransaction",
                [] (JsonObject payload) {}, //ignore req
                [&txId_generate] () {
                    //create conf
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2));
                    JsonObject payload = doc->to<JsonObject>();

                    JsonObject idTagInfo = payload.createNestedObject("idTagInfo");
                    idTagInfo["status"] = "Accepted";
                    txId_generate++;
                    payload["transactionId"] = txId_generate;
                    return doc;
                });});

        getOcppContext()->getOperationRegistry().registerOperation("StopTransaction", [&txId_generate, &txId_confirm] () {
            return new Ocpp16::CustomOperation("StopTransaction",
                [&txId_generate, &txId_confirm] (JsonObject payload) {
                    //receive req
                    REQUIRE( payload["transactionId"].as<int>() == txId_generate );
                    REQUIRE( payload["transactionId"].as<int>() == txId_confirm + 1 );
                    txId_confirm = payload["transactionId"].as<int>();
                }, 
                [] () {
                    //create conf
                    return createEmptyDocument();
                });});

        loopback.setConnected(true);
        loop();

        REQUIRE( txId_confirm == txId_base + MO_TXRECORD_SIZE );
    }

    SECTION("Preboot transactions - charge without tx if limit exceeded") {
        mocpp_deinitialize();

        loopback.setConnected(false);
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));

        declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "PreBootTransactions", true, CONFIGURATION_FN)->setBool(true);
        declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "SilentOfflineTransactions", false, CONFIGURATION_FN)->setBool(true); // don't report further transactions to server but charge anyway
        configuration_save();

        loop();

        for (size_t i = 0; i < MO_TXRECORD_SIZE; i++) {
            beginTransaction_authorized("mIdTag");

            loop();

            REQUIRE(isTransactionRunning());

            endTransaction();

            loop();

            REQUIRE(!isTransactionRunning());
        }

        // now, tx journal is full. Block any further charging session

        auto tx_success = beginTransaction_authorized("mIdTag");
        REQUIRE( tx_success );

        loop();

        REQUIRE(isTransactionRunning());
        REQUIRE(ocppPermitsCharge());

        endTransaction();

        loop();

        // Check if all 4 cached transctions are transmitted after going online

        const int txId_base = 10000;
        int txId_generate = txId_base;
        int txId_confirm = txId_base;

        getOcppContext()->getOperationRegistry().registerOperation("StartTransaction", [&txId_generate] () {
            return new Ocpp16::CustomOperation("StartTransaction",
                [] (JsonObject payload) {}, //ignore req
                [&txId_generate] () {
                    //create conf
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2));
                    JsonObject payload = doc->to<JsonObject>();

                    JsonObject idTagInfo = payload.createNestedObject("idTagInfo");
                    idTagInfo["status"] = "Accepted";
                    txId_generate++;
                    payload["transactionId"] = txId_generate;
                    return doc;
                });});

        getOcppContext()->getOperationRegistry().registerOperation("StopTransaction", [&txId_generate, &txId_confirm] () {
            return new Ocpp16::CustomOperation("StopTransaction",
                [&txId_generate, &txId_confirm] (JsonObject payload) {
                    //receive req
                    REQUIRE( payload["transactionId"].as<int>() == txId_generate );
                    REQUIRE( payload["transactionId"].as<int>() == txId_confirm + 1 );
                    txId_confirm = payload["transactionId"].as<int>();
                }, 
                [] () {
                    //create conf
                    return createEmptyDocument();
                });});

        loopback.setConnected(true);
        loop();

        REQUIRE( txId_confirm == txId_base + MO_TXRECORD_SIZE );
    }

    SECTION("Preboot transactions - mix PreBoot with Offline tx") {
        
        /*
         * The charger boots and connects to the OCPP server normally. It looses connection and then starts
         * transaction #1 which is persisted on flash. Then a power loss occurs, but the charger doesn't reconnect.
         * Start transaction #2 in PreBoot mode. Trigger another power loss, start transaction #3 while still
         * being offline and then, after reconnection to the server, transaction #4.
         * 
         * Tx #1 can be fully restored. The timestamp information for Tx #2 is missing, so it is discarded. Tx #3 is
         * missing absolute timestamps at first, but after reconnection with the server, the timestamps get updated
         * with absolute values from the server. Tx #4 is the standard case for transactions and should start normally.
         */

        // use idTags to identify the transactions
        const char *tx1_idTag = "Tx#1";
        const char *tx2_idTag = "Tx#2";
        const char *tx3_idTag = "Tx#3";
        const char *tx4_idTag = "Tx#4";

        declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "PreBootTransactions", true)->setBool(true);
        declareConfiguration<bool>("AllowOfflineTxForUnknownId", true)->setBool(true);
        configuration_save();
        loop();

        // start Tx #1 (offline tx)
        loopback.setConnected(false);

        MO_DBG_DEBUG("begin tx (%s)", tx1_idTag);
        beginTransaction(tx1_idTag);
        loop();
        REQUIRE(isTransactionRunning());
        endTransaction();
        loop();
        REQUIRE(!isTransactionRunning());

        // first power cycle
        mocpp_deinitialize();
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        loop();

        // start Tx #2 (PreBoot tx, won't get timestamp)

        MO_DBG_DEBUG("begin tx (%s)", tx2_idTag);
        beginTransaction(tx2_idTag);
        loop();
        REQUIRE(isTransactionRunning());
        endTransaction();
        loop();
        REQUIRE(!isTransactionRunning());

        // second power cycle
        mocpp_deinitialize();
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        loop();

        // start Tx #3 (PreBoot tx, will eventually get timestamp)

        MO_DBG_DEBUG("begin tx (%s)", tx3_idTag);
        beginTransaction(tx3_idTag);
        loop();
        REQUIRE(isTransactionRunning());
        endTransaction();
        loop();
        REQUIRE(!isTransactionRunning());

        // set up checks before getting online and starting Tx #4
        bool check_1 = false, check_2 = false, check_3 = false, check_4 = false;

        getOcppContext()->getOperationRegistry().registerOperation("StartTransaction", 
                [&check_1, &check_2, &check_3, &check_4,
                        tx1_idTag, tx2_idTag, tx3_idTag, tx4_idTag] () {
            return new Ocpp16::CustomOperation("StartTransaction",
                [&check_1, &check_2, &check_3, &check_4,
                        tx1_idTag, tx2_idTag, tx3_idTag, tx4_idTag] (JsonObject payload) {
                    //process req
                    const char *idTag = payload["idTag"] | "_Undefined";
                    if (!strcmp(idTag, tx1_idTag )) {
                        check_1 = true;
                    } else if (!strcmp(idTag, tx2_idTag )) {
                        check_2 = true;
                    } else if (!strcmp(idTag, tx3_idTag )) {
                        check_3 = true;
                    } else if (!strcmp(idTag, tx4_idTag )) {
                        check_4 = true;
                    }
                },
                [] () {
                    //create conf
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2));
                    JsonObject payload = doc->to<JsonObject>();

                    JsonObject idTagInfo = payload.createNestedObject("idTagInfo");
                    idTagInfo["status"] = "Accepted";
                    static int uniqueTxId = 1000;
                    payload["transactionId"] = uniqueTxId++; //sample data for debug purpose
                    return doc;
                });});
        
        // get online
        loopback.setConnected(true);
        loop();

        // start Tx #4
        MO_DBG_DEBUG("begin tx (%s)", tx4_idTag);
        beginTransaction(tx4_idTag);
        loop();
        REQUIRE(isTransactionRunning());
        endTransaction();
        loop();
        REQUIRE(!isTransactionRunning());

        // evaluate results
        REQUIRE( check_1 );
        REQUIRE( !check_2 ); // critical data for Tx #2 got lost so it must be discarded
        REQUIRE( check_3 );
        REQUIRE( check_4 );
    }

    SECTION("Set Unavaible"){

        beginTransaction("mIdTag");

        loop();

        auto connector = getOcppContext()->getModel().getConnector(1);
        REQUIRE(connector->getStatus() == ChargePointStatus_Charging);
        REQUIRE(isOperative());

        bool checkProcessed = false;

        auto changeAvailability = makeRequest(new Ocpp16::CustomOperation(
                "ChangeAvailability",
                [] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(2));
                    auto payload = doc->to<JsonObject>();
                    payload["connectorId"] = 1;
                    payload["type"] = "Inoperative";
                    return doc;},
                [&checkProcessed] (JsonObject payload) {
                    //receive conf
                    checkProcessed = true;

                    REQUIRE(!strcmp(payload["status"], "Scheduled"));}));

        getOcppContext()->initiateRequest(std::move(changeAvailability));

        loop();

        REQUIRE(checkProcessed);
        REQUIRE(connector->getStatus() == ChargePointStatus_Charging);
        REQUIRE(isOperative());

        mocpp_deinitialize();

        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        connector = getOcppContext()->getModel().getConnector(1);

        loop();

        REQUIRE(connector->getStatus() == ChargePointStatus_Charging);
        REQUIRE(isOperative());

        endTransaction();

        loop();

        REQUIRE(connector->getStatus() == ChargePointStatus_Unavailable);
        REQUIRE(!isOperative());

        connector->setAvailability(true);

        REQUIRE(connector->getStatus() == ChargePointStatus_Available);
        REQUIRE(isOperative());
    }

    SECTION("UnlockConnector") {
        // UnlockConnector handler

        beginTransaction_authorized("mIdTag");

        loop();
        REQUIRE( isTransactionRunning() );

        bool checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(
            new MicroOcpp::Ocpp16::CustomOperation("UnlockConnector",
                [] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(1));
                    auto payload = doc->to<JsonObject>();
                    payload["connectorId"] = 1;
                    return doc;
                },
                [&checkProcessed] (JsonObject payload) {
                    //process conf
                    checkProcessed = true;
                    REQUIRE( !strcmp(payload["status"] | "_Undefined", "NotSupported") );
                })));
        
        loop();
        REQUIRE( checkProcessed );
        REQUIRE( isTransactionRunning() ); // NotSupported doesn't lead to transaction stop

#if MO_ENABLE_CONNECTOR_LOCK

        setOnUnlockConnectorInOut([] () -> UnlockConnectorResult {
            // connector lock fails
            return UnlockConnectorResult_UnlockFailed;
        });

        checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(
            new MicroOcpp::Ocpp16::CustomOperation("UnlockConnector",
                [] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(1));
                    auto payload = doc->to<JsonObject>();
                    payload["connectorId"] = 1;
                    return doc;
                },
                [&checkProcessed] (JsonObject payload) {
                    //process conf
                    checkProcessed = true;
                    REQUIRE( !strcmp(payload["status"] | "_Undefined", "UnlockFailed") );
                })));
        
        loop();
        REQUIRE( checkProcessed );
        REQUIRE( !isTransactionRunning() ); // Stop tx when UnlockConnector generally supported

        setOnUnlockConnectorInOut([] () -> UnlockConnectorResult {
            // connector lock times out
            return UnlockConnectorResult_Pending;
        });

        checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(
            new MicroOcpp::Ocpp16::CustomOperation("UnlockConnector",
                [] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(1));
                    auto payload = doc->to<JsonObject>();
                    payload["connectorId"] = 1;
                    return doc;
                },
                [&checkProcessed] (JsonObject payload) {
                    //process conf
                    checkProcessed = true;
                    REQUIRE( !strcmp(payload["status"] | "_Undefined", "UnlockFailed") );
                })));
        
        loop();
        mtime += MO_UNLOCK_TIMEOUT; // increment clock so that MO_UNLOCK_TIMEOUT expires
        loop();
        REQUIRE( checkProcessed );

#else
        endTransaction();
        loop();
#endif //MO_ENABLE_CONNECTOR_LOCK

    }

    SECTION("TxStartPoint - PowerPathClosed") {

        declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "TxStartOnPowerPathClosed", true)->setBool(true);

        // precondition: charge not allowed
        REQUIRE( !ocppPermitsCharge() );
        REQUIRE( !isTransactionRunning() );

        setConnectorPluggedInput([] () {return false;}); // TxStartOnPowerPathClosed removes ConnectorPlugged as a prerequisite of transactions
        setEvReadyInput([] () {return false;}); // TxStartOnPowerPathClosed puts EvReady in the role of ConnectorPlugged in conventional transactions

        beginTransaction("mIdTag");

        loop();

        // in contrast to conventional tx mode, charge permission is granted before transaction. PowerPathClosed is a prerequisite of transactions
        REQUIRE( ocppPermitsCharge() );
        REQUIRE( !isTransactionRunning() );

        setConnectorPluggedInput([] () {return true;}); // ConnectorPlugged not sufficient to start tx

        loop();

        REQUIRE( ocppPermitsCharge() );
        REQUIRE( !isTransactionRunning() );

        setEvReadyInput([] () {return true;}); // now, close PowerPath. Transaction will start now

        loop();

        REQUIRE( ocppPermitsCharge() );
        REQUIRE( isTransactionRunning() );

        endTransaction();

        loop();

    }

    SECTION("TransactionMessageAttempts-/RetryInterval") {

        /*
         * Scenarios:
         * - final failure to send txMsg after tx terminated
         * - normal communication restored after a final failure
         * - StartTx fails finally during tx
         * - StartTx works but StopTx fails finally after tx terminated
         * - sends attempts fail until final attempt succeeds
         * - after reboot, continue attempting
         */

        declareConfiguration<int>("TransactionMessageAttempts", 1)->setInt(1);

        bool checkProcessedStartTx = false;
        bool checkProcessedStopTx = false;
        unsigned int txId = 1000;

         /*
         * - final failure to send txMsg after tx terminated
         */

        getOcppContext()->getOperationRegistry().registerOperation("StartTransaction", [&checkProcessedStartTx, &txId] () {
            return new Ocpp16::CustomOperation("StartTransaction",
                [&checkProcessedStartTx] (JsonObject payload) {
                    //receive req
                    checkProcessedStartTx = true;
                },
                [&txId] () {
                    //create conf
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2));
                    JsonObject payload = doc->to<JsonObject>();

                    JsonObject idTagInfo = payload.createNestedObject("idTagInfo");
                    idTagInfo["status"] = "Accepted";
                    payload["transactionId"] = txId++;
                    return doc;
                });});

        getOcppContext()->getOperationRegistry().registerOperation("StopTransaction", [&checkProcessedStopTx] () {
            return new Ocpp16::CustomOperation("StopTransaction",
                [&checkProcessedStopTx] (JsonObject payload) {
                    //receive req
                    checkProcessedStopTx = true;
                }, 
                [] () {
                    //create conf
                    return createEmptyDocument();
                });});

        loopback.setOnline(false);

        REQUIRE( !ocppPermitsCharge() );

        beginTransaction_authorized("mIdTag");
        loop();
        REQUIRE( ocppPermitsCharge() );

        endTransaction();
        loop();
        REQUIRE( !ocppPermitsCharge() );

        mtime += 10 * 60 * 1000; //jump 10 minutes into future

        loopback.setOnline(true);
        loop();

        REQUIRE( !checkProcessedStartTx );
        REQUIRE( !checkProcessedStopTx );

         /*
         * - normal communication restored after a final failure
         */
        checkProcessedStartTx = false;
        checkProcessedStopTx = false;
        
        beginTransaction_authorized("mIdTag");
        loop();
        REQUIRE( ocppPermitsCharge() );
        REQUIRE( checkProcessedStartTx );

        endTransaction();
        loop();
        REQUIRE( !ocppPermitsCharge() );

        REQUIRE( checkProcessedStopTx );

        /*
         * - StartTx fails finally during tx
         */

        checkProcessedStartTx = false;
        checkProcessedStopTx = false;
        
        loopback.setOnline(false);

        REQUIRE( !ocppPermitsCharge() );

        beginTransaction_authorized("mIdTag");
        loop();
        REQUIRE( ocppPermitsCharge() );

        mtime += 10 * 60 * 1000; //jump 10 minutes into future
        loop();
        REQUIRE( !ocppPermitsCharge() );

        loopback.setOnline(true);
        loop();

        REQUIRE( !checkProcessedStartTx );
        REQUIRE( !checkProcessedStopTx );

        /*
         * - StartTx works but StopTx fails finally after tx terminated
         */

        checkProcessedStartTx = false;
        checkProcessedStopTx = false;
        
        beginTransaction_authorized("mIdTag");
        loop();
        REQUIRE( ocppPermitsCharge() );
        REQUIRE( checkProcessedStartTx );

        loopback.setOnline(false);

        endTransaction();
        loop();
        mtime += 10 * 60 * 1000; //jump 10 minutes into future

        loopback.setOnline(true);
        loop();
        REQUIRE( !checkProcessedStopTx );

        /*
         * - sends attempts fail until final attempt succeeds
         */

        const size_t NUM_ATTEMPTS = 3;
        const int RETRY_INTERVAL_SECS = 3600;

        declareConfiguration<int>("TransactionMessageAttempts", 0)->setInt(NUM_ATTEMPTS);
        declareConfiguration<int>("TransactionMessageRetryInterval", 0)->setInt(RETRY_INTERVAL_SECS);

        configuration_save();

        checkProcessedStartTx = false;
        checkProcessedStopTx = false;

        unsigned int attemptNr = 0;

        getOcppContext()->getOperationRegistry().registerOperation("StartTransaction", [&checkProcessedStartTx, &txId, &attemptNr] () {
            return new Ocpp16::CustomOperation("StartTransaction",
                [&attemptNr] (JsonObject payload) {
                    //receive req
                    attemptNr++;
                },
                [&txId] () {
                    //create conf
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2));
                    JsonObject payload = doc->to<JsonObject>();

                    JsonObject idTagInfo = payload.createNestedObject("idTagInfo");
                    idTagInfo["status"] = "Accepted";
                    payload["transactionId"] = txId++;
                    return doc;
                },
                [&attemptNr] () {
                    //ErrorCode for CALLERROR
                    return attemptNr < NUM_ATTEMPTS ? "InternalError" : (const char*)nullptr;
                });});
        
        beginTransaction_authorized("mIdTag");
        loop();
        REQUIRE( ocppPermitsCharge() );
        REQUIRE( attemptNr == 1 );

        mtime += (unsigned long)RETRY_INTERVAL_SECS * 1000;
        loop();
        REQUIRE( ocppPermitsCharge() );
        REQUIRE( attemptNr == 2 );

        mtime += 2 * (unsigned long)RETRY_INTERVAL_SECS * 1000;
        loop();
        REQUIRE( ocppPermitsCharge() );
        REQUIRE( attemptNr == 3 );

        mtime += 100 * (unsigned long)RETRY_INTERVAL_SECS * 1000;
        loop();
        REQUIRE( ocppPermitsCharge() );
        REQUIRE( attemptNr == 3 ); //no further retry after third and successful attempt

        endTransaction();
        loop();
        REQUIRE( !ocppPermitsCharge() );
        REQUIRE( attemptNr == 3 );
        REQUIRE( checkProcessedStopTx );

        /*
         * - after reboot, continue attempting
         */

        getOcppContext()->getModel().getClock().setTime(BASE_TIME); //reset system time to have roughly the same time after reboot

        checkProcessedStartTx = false;
        checkProcessedStopTx = false;
        attemptNr = 0;

        beginTransaction_authorized("mIdTag");
        loop();
        REQUIRE( ocppPermitsCharge() );
        REQUIRE( attemptNr == 1 );

        mocpp_deinitialize();

        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));

        getOcppContext()->getModel().getClock().setTime(BASE_TIME);

        getOcppContext()->getOperationRegistry().registerOperation("StartTransaction", [&checkProcessedStartTx, &txId, &attemptNr] () {
            return new Ocpp16::CustomOperation("StartTransaction",
                [&attemptNr] (JsonObject payload) {
                    //receive req
                    attemptNr++;
                },
                [&txId] () {
                    //create conf
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2));
                    JsonObject payload = doc->to<JsonObject>();

                    JsonObject idTagInfo = payload.createNestedObject("idTagInfo");
                    idTagInfo["status"] = "Accepted";
                    payload["transactionId"] = txId++;
                    return doc;
                },
                [&attemptNr] () {
                    //ErrorCode for CALLERROR
                    return attemptNr < NUM_ATTEMPTS ? "InternalError" : (const char*)nullptr;
                });});

        getOcppContext()->getOperationRegistry().registerOperation("StopTransaction", [&checkProcessedStopTx] () {
            return new Ocpp16::CustomOperation("StopTransaction",
                [&checkProcessedStopTx] (JsonObject payload) {
                    //receive req
                    checkProcessedStopTx = true;
                }, 
                [] () {
                    //create conf
                    return createEmptyDocument();
                });});

        loop();

        REQUIRE( ocppPermitsCharge() );
        REQUIRE( attemptNr == 1 );

        mtime += (unsigned long)RETRY_INTERVAL_SECS * 1000;
        loop();
        REQUIRE( ocppPermitsCharge() );
        REQUIRE( attemptNr == 2 );

        mtime += 2 * (unsigned long)RETRY_INTERVAL_SECS * 1000;
        loop();
        REQUIRE( ocppPermitsCharge() );
        REQUIRE( attemptNr == 3 );

        mtime += 100 * (unsigned long)RETRY_INTERVAL_SECS * 1000;
        loop();
        REQUIRE( ocppPermitsCharge() );
        REQUIRE( attemptNr == 3 ); //no further retry after third and successful attempt

        endTransaction();
        loop();
        REQUIRE( !ocppPermitsCharge() );
        REQUIRE( attemptNr == 3 );
        REQUIRE( checkProcessedStopTx );
    }

    SECTION("StatusNotification") {

        mocpp_deinitialize();

        mocpp_initialize(loopback, ChargerCredentials());

        bool checkProcessed = false;
        const char *checkStatus = "";

        getOcppContext()->getOperationRegistry().setOnRequest("StatusNotification",
            [&checkProcessed, &checkStatus] (JsonObject payload) {
                //process req
                if (payload["connectorId"].as<int>() == 1) {
                    checkProcessed = true;
                    REQUIRE( !strcmp(payload["status"] | "_Undefined", checkStatus) );
                }
            });

        checkStatus = "Available";
        loop();
        REQUIRE( checkProcessed );

        checkStatus = "Preparing";
        checkProcessed = false;
        setConnectorPluggedInput([] () {return true;});
        loop();
        REQUIRE( checkProcessed );

        checkStatus = "Available";
        checkProcessed = false;
        setConnectorPluggedInput([] () {return false;});
        loop();
        REQUIRE( checkProcessed );

        checkStatus = "Preparing";
        checkProcessed = false;
        beginTransaction("mIdTag");
        loop();
        REQUIRE( checkProcessed );

        checkStatus = "Available";
        checkProcessed = false;
        endTransaction("mIdTag");
        loop();
        REQUIRE( checkProcessed );

        checkStatus = "Preparing";
        beginTransaction("mIdTag");
        loop();
        checkProcessed = false;

        checkStatus = "Charging";
        checkProcessed = false;
        setConnectorPluggedInput([] () {return true;});
        loop();
        REQUIRE( checkProcessed );

        checkStatus = "SuspendedEV";
        checkProcessed = false;
        setEvReadyInput([] () {return false;});
        loop();
        REQUIRE( checkProcessed );

        checkStatus = "SuspendedEVSE";
        checkProcessed = false;
        setEvReadyInput([] () {return true;});
        setEvseReadyInput([] () {return false;});
        loop();
        REQUIRE( checkProcessed );

        checkStatus = "Charging";
        checkProcessed = false;
        setEvReadyInput([] () {return true;});
        setEvseReadyInput([] () {return true;});
        loop();
        REQUIRE( checkProcessed );

        checkStatus = "Finishing";
        checkProcessed = false;
        endTransaction();
        loop();
        REQUIRE( checkProcessed );

        checkStatus = "Available";
        checkProcessed = false;
        setConnectorPluggedInput([] () {return false;});
        loop();
        REQUIRE( checkProcessed );

        checkStatus = "Available";
        const char *checkStatus2 = checkStatus;
        checkProcessed = false;
        mocpp_deinitialize();
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        getOcppContext()->getOperationRegistry().setOnRequest("StatusNotification",
            [&checkProcessed, &checkStatus, &checkStatus2] (JsonObject payload) {
                //process req
                if (payload["connectorId"].as<int>() == 1) {
                    checkProcessed = true;
                    REQUIRE( (!strcmp(payload["status"] | "_Undefined", checkStatus) || !strcmp(payload["status"] | "_Undefined", checkStatus2)) );
                }
            });
        loop();
        REQUIRE( checkProcessed );

        checkStatus = "Charging";
        checkStatus2 = "Preparing";
        checkProcessed = false;
        beginTransaction("mIdTag");
        loop();
        REQUIRE( checkProcessed );

        checkStatus = "Charging";
        checkStatus2 = checkStatus;
        checkProcessed = false;
        mocpp_deinitialize();
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        getOcppContext()->getOperationRegistry().setOnRequest("StatusNotification",
            [&checkProcessed, &checkStatus] (JsonObject payload) {
                //process req
                if (payload["connectorId"].as<int>() == 1) {
                    checkProcessed = true;
                    REQUIRE( !strcmp(payload["status"] | "_Undefined", checkStatus) );
                }
            });
        loop();
        REQUIRE( checkProcessed );

        checkStatus = "Available";
        checkStatus2 = checkStatus;
        checkProcessed = false;
        endTransaction();
        mocpp_deinitialize();
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        getOcppContext()->getOperationRegistry().setOnRequest("StatusNotification",
            [&checkProcessed, &checkStatus] (JsonObject payload) {
                //process req
                if (payload["connectorId"].as<int>() == 1) {
                    checkProcessed = true;
                    REQUIRE( !strcmp(payload["status"] | "_Undefined", checkStatus) );
                }
            });
        loop();
        REQUIRE( checkProcessed );
    }

    SECTION("No filesystem access behavior") {

        //re-init without filesystem access
        mocpp_deinitialize();

        mocpp_initialize(loopback, ChargerCredentials(), MicroOcpp::makeDefaultFilesystemAdapter(MicroOcpp::FilesystemOpt::Deactivate));
        mocpp_set_timer(custom_timer_cb);
        
        loop();

        REQUIRE( getChargePointStatus() == ChargePointStatus_Available );
        REQUIRE( !ocppPermitsCharge() );

        for (size_t i = 0; i < 3; i++) {

            beginTransaction("mIdTag");
            loop();

            REQUIRE( getChargePointStatus() == ChargePointStatus_Charging );
            REQUIRE( ocppPermitsCharge() );

            endTransaction();
            loop();

            REQUIRE( getChargePointStatus() == ChargePointStatus_Available );
            REQUIRE( !ocppPermitsCharge() );
        }

        //Tx status will be lost over reboot

        beginTransaction("mIdTag");
        loop();

        REQUIRE( getChargePointStatus() == ChargePointStatus_Charging );
        REQUIRE( ocppPermitsCharge() );

        mocpp_deinitialize();

        mocpp_initialize(loopback, ChargerCredentials(), MicroOcpp::makeDefaultFilesystemAdapter(MicroOcpp::FilesystemOpt::Deactivate));
        mocpp_set_timer(custom_timer_cb);

        loop();

        REQUIRE( getChargePointStatus() == ChargePointStatus_Available );
        REQUIRE( !ocppPermitsCharge() );

        //Note: queueing offline transactions without FS is currently not implemented
    }

    mocpp_deinitialize();
}
