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
#include <MicroOcpp/Model/Authorization/AuthorizationService.h>


#define BASE_TIME "2023-01-01T00:00:00.000Z"

using namespace MicroOcpp;

void generateAuthList(JsonArray out, size_t size, bool compact) {
    for (size_t i = 0; i < size; i++) {
        JsonObject authData = out.createNestedObject();
        JsonObject idTagInfo;
        if (compact) {
            //flat structure
            idTagInfo = authData;
        } else {
            //nested idTagInfo
            idTagInfo = authData["idTagInfo"].to<JsonObject>();
        }

        char buf [IDTAG_LEN_MAX + 1];
        sprintf(buf, "mIdTag%zu", i);
        authData[AUTHDATA_KEY_IDTAG(compact)] = buf;
        idTagInfo[AUTHDATA_KEY_STATUS(compact)] = "Accepted";
    }
}

TEST_CASE( "LocalAuth" ) {
    printf("\nRun %s\n",  "LocalAuth");

    //clean state
    auto filesystem = makeDefaultFilesystemAdapter(FilesystemOpt::Use_Mount_FormatOnFail);
    FilesystemUtils::remove_if(filesystem, [] (const char*) {return true;});

    //initialize Context with dummy socket
    LoopbackConnection loopback;

    mocpp_set_timer(custom_timer_cb);

    mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
    auto& model = getOcppContext()->getModel();
    auto authService = model.getAuthorizationService();
    auto connector = model.getConnector(1);
    model.getClock().setTime(BASE_TIME);

    loop();
    
    //enable local auth
    declareConfiguration<bool>("LocalAuthListEnabled", true)->setBool(true);

    //set Authorize timeout after which the charger is considered offline
    const unsigned long AUTH_TIMEOUT_MS = 60000;
    MicroOcpp::declareConfiguration<int>(MO_CONFIG_EXT_PREFIX "AuthorizationTimeout", -1)->setInt(AUTH_TIMEOUT_MS / 1000);

    //fetch local auth configs
    auto localAuthorizeOffline = declareConfiguration<bool>("LocalAuthorizeOffline", false);
    auto localPreAuthorize = declareConfiguration<bool>("LocalPreAuthorize", false);

    SECTION("Basic local auth - LocalPreAuthorize") {

        localAuthorizeOffline->setBool(false);
        localPreAuthorize->setBool(true);
        
        //set local list
        StaticJsonDocument<256> localAuthList;
        localAuthList[0]["idTag"] = "mIdTag";
        localAuthList[0]["idTagInfo"]["status"] = "Accepted";

        REQUIRE( authService->updateLocalList(localAuthList.as<JsonArray>(), 1, false) );

        REQUIRE( authService->getLocalListSize() == 1 );
        REQUIRE( authService->getLocalAuthorization("mIdTag") != nullptr );
        REQUIRE( authService->getLocalAuthorization("mIdTag")->getAuthorizationStatus() == AuthorizationStatus::Accepted );

        //check TX notification
        bool checkTxAuthorized = false;
        setTxNotificationOutput([&checkTxAuthorized] (Transaction*, TxNotification txNotification) {
            if (txNotification == TxNotification::Authorized) {
                checkTxAuthorized = true;
            }
        });

        //begin transaction and delay Authorize request - tx should start immediately
        loopback.setConnected(false); //Authorize delayed by short offline period

        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        beginTransaction("mIdTag");
        loop();

        REQUIRE( connector->getStatus() == ChargePointStatus::Charging );
        REQUIRE( checkTxAuthorized );

        loopback.setConnected(true);
        endTransaction();
        loop();

        //begin transaction delay Authorize request, but idTag doesn't match local list - tx should start when online again
        checkTxAuthorized = false;
        loopback.setConnected(false);

        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        beginTransaction("wrong idTag");
        loop();

        REQUIRE( connector->getStatus() == ChargePointStatus::Preparing );
        REQUIRE( !checkTxAuthorized );

        loopback.setConnected(true);
        loop();

        REQUIRE( connector->getStatus() == ChargePointStatus::Charging );
        REQUIRE( checkTxAuthorized );

        endTransaction();
        loop();
    }

    SECTION("Basic local auth - LocalAuthorizeOffline") {

        localAuthorizeOffline->setBool(true);
        localPreAuthorize->setBool(false);

        //set local list
        StaticJsonDocument<256> localAuthList;
        localAuthList[0]["idTag"] = "mIdTag";
        localAuthList[0]["idTagInfo"]["status"] = "Accepted";
        authService->updateLocalList(localAuthList.as<JsonArray>(), 1, false);

        //check TX notification
        bool checkTxAuthorized = false;
        setTxNotificationOutput([&checkTxAuthorized] (Transaction*, TxNotification txNotification) {
            if (txNotification == TxNotification::Authorized) {
                checkTxAuthorized = true;
            }
        });

        //make charger offline and begin tx - tx should begin after Authorize timeout
        loopback.setConnected(false);

        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        ulong t_before = mocpp_tick_ms();

        beginTransaction("mIdTag");
        loop();

        REQUIRE( mocpp_tick_ms() - t_before < AUTH_TIMEOUT_MS ); //if this fails, increase AUTH_TIMEOUT_MS
        REQUIRE( connector->getStatus() == ChargePointStatus::Preparing );
        REQUIRE( !checkTxAuthorized );

        mtime += AUTH_TIMEOUT_MS - (mocpp_tick_ms() - t_before); //increment clock so that auth timeout is exceeded
        loop();

        REQUIRE( connector->getStatus() == ChargePointStatus::Charging );
        REQUIRE( checkTxAuthorized );

        loopback.setConnected(true);
        endTransaction();
        loop();

        //make charger offline and begin tx, but idTag doesn't match - tx should be aborted
        bool checkTxTimeout = false;
        setTxNotificationOutput([&checkTxTimeout] (Transaction*, TxNotification txNotification) {
            if (txNotification == TxNotification::AuthorizationTimeout) {
                checkTxTimeout = true;
            }
        });
        loopback.setConnected(false);

        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        t_before = mocpp_tick_ms();

        beginTransaction("wrong idTag");
        loop();

        REQUIRE( connector->getStatus() == ChargePointStatus::Preparing );
        REQUIRE( !checkTxTimeout );

        mtime += AUTH_TIMEOUT_MS - (mocpp_tick_ms() - t_before); //increment clock so that auth timeout is exceeded
        loop();

        REQUIRE( connector->getStatus() == ChargePointStatus::Available );
        REQUIRE( checkTxTimeout );

        loopback.setConnected(true);
        loop();
    }

    SECTION("Basic local auth - AllowOfflineTxForUnknownId") {

        localAuthorizeOffline->setBool(false);
        localPreAuthorize->setBool(false);
        MicroOcpp::declareConfiguration<bool>("AllowOfflineTxForUnknownId", true)->setBool(true);

        //check TX notification
        bool checkTxAuthorized = false;
        setTxNotificationOutput([&checkTxAuthorized] (Transaction*, TxNotification txNotification) {
            if (txNotification == TxNotification::Authorized) {
                checkTxAuthorized = true;
            }
        });

        //make charger offline and begin tx - tx should begin after Authorize timeout
        loopback.setConnected(false);

        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        ulong t_before = mocpp_tick_ms();

        beginTransaction("unknownIdTag");
        loop();

        REQUIRE( connector->getStatus() == ChargePointStatus::Preparing );
        REQUIRE( !checkTxAuthorized );

        mtime += AUTH_TIMEOUT_MS - (mocpp_tick_ms() - t_before); //increment clock so that auth timeout is exceeded
        loop();

        REQUIRE( connector->getStatus() == ChargePointStatus::Charging );
        REQUIRE( checkTxAuthorized );

        loopback.setConnected(true);
        endTransaction();
        loop();
    }

    SECTION("Local auth list entry expired / unauthorized") {

        localPreAuthorize->setBool(true);

        //set local list with expired / unauthorized entry
        StaticJsonDocument<512> localAuthList;
        localAuthList[0]["idTag"] = "mIdTagExpired";
        localAuthList[0]["idTagInfo"]["status"] = "Accepted";
        localAuthList[0]["idTagInfo"]["expiryDate"] = BASE_TIME; //is in past
        localAuthList[1]["idTag"] = "mIdTagUnauthorized";
        localAuthList[1]["idTagInfo"]["status"] = "Blocked";
        authService->updateLocalList(localAuthList.as<JsonArray>(), 1, false);
        REQUIRE( authService->getLocalAuthorization("mIdTagExpired") );
        REQUIRE( authService->getLocalAuthorization("mIdTagUnauthorized") );

        REQUIRE( authService->getLocalAuthorization("mIdTagExpired") );

        //begin transaction and delay Authorize request - cannot PreAuthorize because entry is expired
        loopback.setConnected(false); //Authorize delayed by short offline period

        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        beginTransaction("mIdTagExpired");
        loop();

        REQUIRE( connector->getStatus() == ChargePointStatus::Preparing );

        loopback.setConnected(true);
        loop();

        REQUIRE( connector->getStatus() == ChargePointStatus::Charging );

        endTransaction();
        loop();

        //begin transaction and delay Authorize request - cannot PreAuthorize because entry is unauthorized
        loopback.setConnected(false); //Authorize delayed by short offline period

        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        beginTransaction("mIdTagUnauthorized");
        loop();

        REQUIRE( connector->getStatus() == ChargePointStatus::Preparing );

        loopback.setConnected(true);
        loop();

        REQUIRE( connector->getStatus() == ChargePointStatus::Charging );
        endTransaction();
        loop();
    }

    SECTION("Mix local authorization modes") {

        localAuthorizeOffline->setBool(true);
        localPreAuthorize->setBool(true);
        MicroOcpp::declareConfiguration<bool>("AllowOfflineTxForUnknownId", true)->setBool(true);

        //set local list with accepted and accepted entry
        StaticJsonDocument<512> localAuthList;
        localAuthList[0]["idTag"] = "mIdTagExpired";
        localAuthList[0]["idTagInfo"]["status"] = "Accepted";
        localAuthList[0]["idTagInfo"]["expiryDate"] = BASE_TIME; //is in past
        localAuthList[1]["idTag"] = "mIdTagAccepted";
        localAuthList[1]["idTagInfo"]["status"] = "Accepted";
        authService->updateLocalList(localAuthList.as<JsonArray>(), 1, false);
        REQUIRE( authService->getLocalAuthorization("mIdTagExpired") );
        REQUIRE( authService->getLocalAuthorization("mIdTagAccepted") );

        //begin transaction and delay Authorize request - tx should start immediately
        loopback.setConnected(false);

        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        beginTransaction("mIdTagAccepted");
        loop();

        REQUIRE( connector->getStatus() == ChargePointStatus::Charging );

        loopback.setConnected(true);
        endTransaction();
        loop();

        //begin transaction, but idTag is expired - AllowOfflineTxForUnknownId must not apply
        loopback.setConnected(false);

        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        ulong t_before = mocpp_tick_ms();

        beginTransaction("mIdTagExpired");
        loop();

        REQUIRE( mocpp_tick_ms() - t_before < AUTH_TIMEOUT_MS ); //if this fails, increase AUTH_TIMEOUT_MS
        REQUIRE( connector->getStatus() == ChargePointStatus::Preparing );

        mtime += AUTH_TIMEOUT_MS - (mocpp_tick_ms() - t_before); //increment clock so that auth timeout is exceeded
        loop();

        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        loopback.setConnected(true);
        loop();
    }

    SECTION("DeAuthorize locally authorized tx") {

        localAuthorizeOffline->setBool(false);
        localPreAuthorize->setBool(true);

        //check TX notification
        bool checkTxAuthorized = false;
        setTxNotificationOutput([&checkTxAuthorized] (Transaction*, TxNotification txNotification) {
            if (txNotification == TxNotification::Authorized) {
                checkTxAuthorized = true;
            }
        });

        //set local list
        StaticJsonDocument<256> localAuthList;
        localAuthList[0]["idTag"] = "mIdTag";
        localAuthList[0]["idTagInfo"]["status"] = "Accepted";
        authService->updateLocalList(localAuthList.as<JsonArray>(), 1, false);

        //patch Authorize so it will reject all idTags
        getOcppContext()->getOperationRegistry().registerOperation("Authorize", [] () {
            return new Ocpp16::CustomOperation("Authorize",
                [] (JsonObject) {}, //ignore req
                [] () {
                    //create conf
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(2 * JSON_OBJECT_SIZE(1)));
                    auto payload = doc->to<JsonObject>();
                    payload["idTagInfo"]["status"] = "Blocked";
                    return doc;
                });});

        //begin transaction and delay Authorize request - tx should start immediately
        loopback.setConnected(false); //Authorize delayed by short offline period

        REQUIRE( connector->getStatus() == ChargePointStatus::Available );

        beginTransaction("mIdTag");
        loop();

        REQUIRE( connector->getStatus() == ChargePointStatus::Charging );
        REQUIRE( checkTxAuthorized );

        //check TX notification
        bool checkTxRejected = false;
        setTxNotificationOutput([&checkTxRejected] (Transaction*, TxNotification txNotification) {
            if (txNotification == TxNotification::AuthorizationRejected) {
                checkTxRejected = true;
            }
        });

        loopback.setConnected(true);
        loop();

        REQUIRE( connector->getStatus() == ChargePointStatus::Available );
        REQUIRE( checkTxRejected );

        loop();
    }

    SECTION("LocalListConflict") {

        //patch Authorize so it will reject all idTags
        bool checkAuthorize = false;
        getOcppContext()->getOperationRegistry().registerOperation("Authorize", [&checkAuthorize] () {
            return new Ocpp16::CustomOperation("Authorize",
                [&checkAuthorize] (JsonObject) {
                    checkAuthorize = true;
                },
                [] () {
                    //create conf
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(2 * JSON_OBJECT_SIZE(1)));
                    auto payload = doc->to<JsonObject>();
                    payload["idTagInfo"]["status"] = "Blocked";
                    return doc;
                });});
        
        //patch StartTransaction so it will DeAuthorize all txs
        bool checkStartTx = false;
        getOcppContext()->getOperationRegistry().registerOperation("StartTransaction", [&checkStartTx] () {
            return new Ocpp16::CustomOperation("StartTransaction",
                [&checkStartTx] (JsonObject) {
                    checkStartTx = true;
                },
                [] () {
                    //create conf
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
                            JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(1)));
                    auto payload = doc->to<JsonObject>();
                    payload["idTagInfo"]["status"] = "Blocked";
                    payload["transactionId"] = 1000;
                    return doc;
                });});
        
        //check resulting StatusNotification message
        bool checkLocalListConflict = false;
        getOcppContext()->getOperationRegistry().registerOperation("StatusNotification", [&checkLocalListConflict] () {
            return new Ocpp16::CustomOperation("StatusNotification",
                [&checkLocalListConflict] (JsonObject payload) {
                    if (payload["connectorId"] == 0 &&
                            !strcmp(payload["errorCode"] | "_Undefined", "LocalListConflict")) {
                        checkLocalListConflict = true;
                    }
                }, 
                [] () {
                    //create conf
                    return createEmptyDocument();
                });});

        //set local list
        StaticJsonDocument<256> localAuthList;
        localAuthList[0]["idTag"] = "mIdTag";
        localAuthList[0]["idTagInfo"]["status"] = "Accepted";
        authService->updateLocalList(localAuthList.as<JsonArray>(), 1, false);

        //send Authorize and StartTx and check if they trigger LocalListConflict
        beginTransaction("mIdTag");
        loop();
        REQUIRE( checkLocalListConflict );
        REQUIRE( checkAuthorize );
        REQUIRE( !checkStartTx );

        checkLocalListConflict = false;
        checkAuthorize = false;
        beginTransaction_authorized("mIdTag");
        loop();
        REQUIRE( checkLocalListConflict );
        REQUIRE( !checkAuthorize );
        REQUIRE( checkStartTx );
    }

    SECTION("Update local list") {

        REQUIRE( authService->getLocalListSize() == 0 ); //idle, empty local list

        //set local list
        int localListVersion = 42;
        StaticJsonDocument<256> localAuthList;
        localAuthList[0]["idTag"] = "mIdTag";
        localAuthList[0]["idTagInfo"]["status"] = "Accepted";
        authService->updateLocalList(localAuthList.as<JsonArray>(), localListVersion, false);
        REQUIRE( authService->getLocalListVersion() == localListVersion );
        REQUIRE( authService->getLocalListSize() == 1 );
        REQUIRE( authService->getLocalAuthorization("mIdTag") != nullptr );

        //overwrite list
        localListVersion++;
        localAuthList.clear();
        localAuthList[0]["idTag"] = "mIdTag2";
        localAuthList[0]["idTagInfo"]["status"] = "Accepted";
        authService->updateLocalList(localAuthList.as<JsonArray>(), localListVersion, false);
        REQUIRE( authService->getLocalListVersion() == localListVersion );
        REQUIRE( authService->getLocalListSize() == 1 );
        REQUIRE( authService->getLocalAuthorization("mIdTag") == nullptr );
        REQUIRE( authService->getLocalAuthorization("mIdTag2") != nullptr );

        //reset list
        localListVersion++;
        localAuthList.clear();
        localAuthList.to<JsonArray>();
        authService->updateLocalList(localAuthList.as<JsonArray>(), localListVersion, false);
        REQUIRE( authService->getLocalListVersion() == 0 ); //localListVersion is ignored - empty list always resets version
        REQUIRE( authService->getLocalListSize() == 0 );

        //consecutive updates in Differential mode
        localListVersion = 1;
        localAuthList.clear();
        localAuthList[0]["idTag"] = "mIdTag";
        localAuthList[0]["idTagInfo"]["status"] = "Accepted";
        authService->updateLocalList(localAuthList.as<JsonArray>(), localListVersion, true);
        REQUIRE( authService->getLocalListVersion() == localListVersion );
        REQUIRE( authService->getLocalListSize() == 1 );

        //append further entry
        localListVersion++;
        localAuthList.clear();
        localAuthList[0]["idTag"] = "mIdTag2";
        localAuthList[0]["idTagInfo"]["status"] = "Accepted";
        authService->updateLocalList(localAuthList.as<JsonArray>(), localListVersion, true);
        REQUIRE( authService->getLocalListVersion() == localListVersion );
        REQUIRE( authService->getLocalListSize() == 2 );

        //overwrite previous entry
        REQUIRE( authService->getLocalAuthorization("mIdTag")->getAuthorizationStatus() == AuthorizationStatus::Accepted );
        localListVersion++;
        localAuthList.clear();
        localAuthList[0]["idTag"] = "mIdTag";
        localAuthList[0]["idTagInfo"]["status"] = "Blocked";
        authService->updateLocalList(localAuthList.as<JsonArray>(), localListVersion, true);
        REQUIRE( authService->getLocalListVersion() == localListVersion );
        REQUIRE( authService->getLocalListSize() == 2 );
        REQUIRE( authService->getLocalAuthorization("mIdTag")->getAuthorizationStatus() == AuthorizationStatus::Blocked );

        //empty update keeps previous entries
        localListVersion++;
        localAuthList.clear();
        localAuthList.to<JsonArray>();
        authService->updateLocalList(localAuthList.as<JsonArray>(), localListVersion, true);
        REQUIRE( authService->getLocalListVersion() == localListVersion );
        REQUIRE( authService->getLocalListSize() == 2 );

        //delete entries
        localListVersion++;
        localAuthList.clear();
        localAuthList[0]["idTag"] = "mIdTag";
        authService->updateLocalList(localAuthList.as<JsonArray>(), localListVersion, true);
        REQUIRE( authService->getLocalListVersion() == localListVersion );
        REQUIRE( authService->getLocalListSize() == 1 );

        localListVersion++;
        localAuthList.clear();
        localAuthList[0]["idTag"] = "mIdTag2";
        authService->updateLocalList(localAuthList.as<JsonArray>(), localListVersion, true);
        REQUIRE( authService->getLocalListVersion() == 0 );
        REQUIRE( authService->getLocalListSize() == 0 );
    }

    SECTION("LocalList persistency") {

        int listVersion = 42;

        StaticJsonDocument<512> localAuthList;
        localAuthList[0]["idTag"] = "mIdTagMinimal";
        localAuthList[0]["idTagInfo"]["status"] = "Accepted";
        localAuthList[1]["idTag"] = "mIdTagFull";
        localAuthList[1]["idTagInfo"]["expiryDate"] = BASE_TIME;
        localAuthList[1]["idTagInfo"]["parentIdTag"] = "mParentIdTag";
        localAuthList[1]["idTagInfo"]["status"] = "Blocked";
        authService->updateLocalList(localAuthList.as<JsonArray>(), listVersion, false);

        mocpp_deinitialize();

        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        authService = getOcppContext()->getModel().getAuthorizationService();

        REQUIRE( authService->getLocalListVersion() == listVersion );
        REQUIRE( authService->getLocalListSize() == 2 );
        auto auth0 = authService->getLocalAuthorization("mIdTagMinimal");
        REQUIRE( auth0 != nullptr );
        REQUIRE( !strcmp(auth0->getIdTag(), "mIdTagMinimal") );
        REQUIRE( auth0->getExpiryDate() == nullptr );
        REQUIRE( auth0->getParentIdTag() == nullptr );
        REQUIRE( auth0->getAuthorizationStatus() == AuthorizationStatus::Accepted );
        
        auto auth1 = authService->getLocalAuthorization("mIdTagFull");
        REQUIRE( auth1 != nullptr );
        REQUIRE( !strcmp(auth1->getIdTag(), "mIdTagFull") );
        REQUIRE( auth1->getExpiryDate() != nullptr );
        Timestamp baseTimeParsed;
        baseTimeParsed.setTime(BASE_TIME);
        REQUIRE( *auth1->getExpiryDate() == baseTimeParsed );
        REQUIRE( !strcmp(auth1->getParentIdTag(), "mParentIdTag") );
        REQUIRE( auth1->getAuthorizationStatus() == AuthorizationStatus::Blocked );
    }

    SECTION("SendLocalList") {

        int listVersion = 42;
        size_t listSize = 2;
        std::string populatedEntryIdTag; //local auth list entry to be fully populated
        
        //Full update - happy path
        bool checkAccepted = false;
        getOcppContext()->initiateRequest(makeRequest(
            new Ocpp16::CustomOperation("SendLocalList",
                [&listVersion, &listSize, &populatedEntryIdTag] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
                            4096));
                    auto payload = doc->to<JsonObject>();
                    payload["listVersion"] = listVersion;
                    generateAuthList(payload["localAuthorizationList"].to<JsonArray>(), listSize, false);

                    //fully populate first entry
                    populatedEntryIdTag = payload["localAuthorizationList"][0]["idTag"] | "_Undefined";
                    payload["localAuthorizationList"][0]["idTagInfo"]["expiryDate"] = BASE_TIME;
                    payload["localAuthorizationList"][0]["idTagInfo"]["parentIdTag"] = "mParentIdTag";
                    payload["localAuthorizationList"][0]["idTagInfo"]["status"] = "Blocked";
                    payload["updateType"] = "Full";
                    return doc;
                },
                [&checkAccepted] (JsonObject payload) {
                    //process conf
                    checkAccepted = !strcmp(payload["status"] | "_Undefined", "Accepted");
                })));
        
        loop();
        REQUIRE( authService->getLocalListVersion() == listVersion );
        REQUIRE( authService->getLocalListSize() == listSize );
        REQUIRE( !populatedEntryIdTag.empty() );
        auto localAuth = authService->getLocalAuthorization(populatedEntryIdTag.c_str());
        REQUIRE( localAuth != nullptr );
        Timestamp baseTimeParsed;
        baseTimeParsed.setTime(BASE_TIME);
        REQUIRE( localAuth->getExpiryDate() );
        REQUIRE( *localAuth->getExpiryDate() == baseTimeParsed );
        REQUIRE( !strcmp(localAuth->getIdTag(), populatedEntryIdTag.c_str()) );
        REQUIRE( !strcmp(localAuth->getParentIdTag(), "mParentIdTag") );
        REQUIRE( localAuth->getAuthorizationStatus() == AuthorizationStatus::Blocked );
        REQUIRE( checkAccepted );

        //Differential update - happy path
        listVersion++;
        listSize++; //add one extra entry and update all others

        checkAccepted = false;
        getOcppContext()->initiateRequest(makeRequest(
            new Ocpp16::CustomOperation("SendLocalList",
                [&listVersion, &listSize] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
                            1024));
                    auto payload = doc->to<JsonObject>();
                    payload["listVersion"] = listVersion;
                    generateAuthList(payload["localAuthorizationList"].to<JsonArray>(), listSize, false);
                    payload["updateType"] = "Differential";
                    return doc;
                },
                [&checkAccepted] (JsonObject payload) {
                    //process conf
                    checkAccepted = !strcmp(payload["status"] | "_Undefined", "Accepted");
                })));
        
        loop();
        REQUIRE( authService->getLocalListVersion() == listVersion );
        REQUIRE( authService->getLocalListSize() == listSize );
        REQUIRE( checkAccepted );

        //Differential update - version mismatch
        size_t listSizeInvalid = listSize + 1;

        bool checkVersionMismatch = false;
        getOcppContext()->initiateRequest(makeRequest(
            new Ocpp16::CustomOperation("SendLocalList",
                [&listVersion, &listSizeInvalid] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
                            1024));
                    auto payload = doc->to<JsonObject>();
                    payload["listVersion"] = listVersion;
                    generateAuthList(payload["localAuthorizationList"].to<JsonArray>(), listSizeInvalid, false);
                    payload["updateType"] = "Differential";
                    return doc;
                },
                [&checkVersionMismatch] (JsonObject payload) {
                    //process conf
                    checkVersionMismatch = !strcmp(payload["status"] | "_Undefined", "VersionMismatch");
                })));
        
        loop();
        REQUIRE( authService->getLocalListVersion() == listVersion );
        REQUIRE( authService->getLocalListSize() == listSize );
        REQUIRE( checkVersionMismatch );

        //Boundary check - maximum entries per SendLocalList
        listVersion = 42;
        listSize = (size_t) declareConfiguration<int>("SendLocalListMaxLength", -1)->getInt();

        checkAccepted = false;
        getOcppContext()->initiateRequest(makeRequest(
            new Ocpp16::CustomOperation("SendLocalList",
                [&listVersion, &listSize] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
                            4096));
                    auto payload = doc->to<JsonObject>();
                    payload["listVersion"] = listVersion;
                    generateAuthList(payload["localAuthorizationList"].to<JsonArray>(), listSize, false);
                    payload["updateType"] = "Full";
                    return doc;
                },
                [&checkAccepted] (JsonObject payload) {
                    //process conf
                    checkAccepted = !strcmp(payload["status"] | "_Undefined", "Accepted");
                })));
        
        loop();
        REQUIRE( authService->getLocalListVersion() == listVersion );
        REQUIRE( authService->getLocalListSize() == listSize );
        REQUIRE( checkAccepted );

        //Boundary check - maximum entries per SendLocalList in Differential mode
        listVersion++;

        checkAccepted = false;
        getOcppContext()->initiateRequest(makeRequest(
            new Ocpp16::CustomOperation("SendLocalList",
                [&listVersion, &listSize] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
                            4096));
                    auto payload = doc->to<JsonObject>();
                    payload["listVersion"] = listVersion;
                    generateAuthList(payload["localAuthorizationList"].to<JsonArray>(), listSize, false);
                    payload["updateType"] = "Differential";
                    return doc;
                },
                [&checkAccepted] (JsonObject payload) {
                    //process conf
                    checkAccepted = !strcmp(payload["status"] | "_Undefined", "Accepted");
                })));
        
        loop();
        REQUIRE( authService->getLocalListVersion() == listVersion );
        REQUIRE( authService->getLocalListSize() == listSize );
        REQUIRE( checkAccepted );

        //Boundary check - exceed maximum entries per SendLocalList
        int listVersionInvalid = listVersion + 1;
        listSizeInvalid = listSize + 1;

        bool errOccurence = false;
        getOcppContext()->initiateRequest(makeRequest(
            new Ocpp16::CustomOperation("SendLocalList",
                [&listVersionInvalid, &listSizeInvalid] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
                            4096));
                    auto payload = doc->to<JsonObject>();
                    payload["listVersion"] = listVersionInvalid;
                    generateAuthList(payload["localAuthorizationList"].to<JsonArray>(), listSizeInvalid, false);
                    payload["updateType"] = "Full";
                    return doc;
                },
                [] (JsonObject) { }, //ignore conf
                [&errOccurence] (const char *errorCode, const char*, JsonObject) {
                    errOccurence = !strcmp(errorCode, "OccurenceConstraintViolation");
                    return true;
                })));
        
        loop();
        REQUIRE( authService->getLocalListVersion() == listVersion );
        REQUIRE( authService->getLocalListSize() == listSize );
        REQUIRE( errOccurence );

        //Boundary check - exceed maximum local list size by multiple Differerntial updates

        //clear local list
        StaticJsonDocument<64> emptyDoc;
        emptyDoc.to<JsonArray>();
        authService->updateLocalList(emptyDoc.as<JsonArray>(), 1, false);

        size_t localAuthListMaxLength = (size_t) declareConfiguration<int>("LocalAuthListMaxLength", -1)->getInt();

        //send Differential lists with 2 entries: update an existing entry and add a new entry
        for (size_t i = 1; i < localAuthListMaxLength; i++) {
            //Full update - happy path
            bool checkAccepted = false;
            getOcppContext()->initiateRequest(makeRequest(
                new Ocpp16::CustomOperation("SendLocalList",
                    [&i] () {
                        //create req
                        auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
                                1024));
                        auto payload = doc->to<JsonObject>();
                        payload["listVersion"] = (int) i;

                        char buf [IDTAG_LEN_MAX + 1];
                        sprintf(buf, "mIdTag%zu", i-1);
                        payload["localAuthorizationList"][0]["idTag"] = buf;
                        payload["localAuthorizationList"][0]["idTagInfo"]["status"] = "Accepted";

                        sprintf(buf, "mIdTag%zu", i);
                        payload["localAuthorizationList"][1]["idTag"] = buf;
                        payload["localAuthorizationList"][1]["idTagInfo"]["status"] = "Accepted";

                        payload["updateType"] = "Differential";
                        return doc;
                    },
                    [&checkAccepted] (JsonObject payload) {
                        //process conf
                        checkAccepted = !strcmp(payload["status"] | "_Undefined", "Accepted");
                    })));
            
            loop();
            REQUIRE( authService->getLocalListVersion() == i );
            REQUIRE( authService->getLocalListSize() == i + 1 );
            REQUIRE( checkAccepted );
        }

        //now exceed local list by sending overflow entry
        listVersion = authService->getLocalListVersion();
        listVersionInvalid = listVersion + 1;
        listSize = authService->getLocalListSize();

        bool checkFailed = false;
        getOcppContext()->initiateRequest(makeRequest(
            new Ocpp16::CustomOperation("SendLocalList",
                [&listVersionInvalid] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
                            1024));
                    auto payload = doc->to<JsonObject>();
                    payload["listVersion"] = listVersionInvalid;

                    //update already existing entry
                    char buf [IDTAG_LEN_MAX + 1];
                    sprintf(buf, "mIdTag%zu", 0UL);
                    payload["localAuthorizationList"][0]["idTag"] = buf;
                    payload["localAuthorizationList"][0]["idTagInfo"]["status"] = "Accepted";

                    //additional overflowing entry
                    payload["localAuthorizationList"][1]["idTag"] = "overflow idTag";
                    payload["localAuthorizationList"][1]["idTagInfo"]["status"] = "Accepted";

                    payload["updateType"] = "Differential";
                    return doc;
                },
                [&checkFailed] (JsonObject payload) {
                    //process conf
                    checkFailed = !strcmp(payload["status"] | "_Undefined", "Failed");
                })));
        loop();
        REQUIRE( authService->getLocalListVersion() == listVersion );
        REQUIRE( authService->getLocalListSize() == listSize );
        REQUIRE( checkFailed );
    }

    SECTION("GetLocalListVersion") {

        int localListVersion = 42;
        StaticJsonDocument<256> localAuthList;
        localAuthList[0]["idTag"] = "mIdTag";
        localAuthList[0]["idTagInfo"]["status"] = "Accepted";
        authService->updateLocalList(localAuthList.as<JsonArray>(), localListVersion, false);

        int checkListVerion = -1;
        getOcppContext()->initiateRequest(makeRequest(
            new Ocpp16::CustomOperation("GetLocalListVersion",
                [] () {
                    //create req
                    return createEmptyDocument();
                },
                [&checkListVerion] (JsonObject payload) {
                    //process conf
                    checkListVerion = payload["listVersion"] | -1;
                })));

        loop();
        REQUIRE( checkListVerion == localListVersion );
    }

    mocpp_deinitialize();
}
