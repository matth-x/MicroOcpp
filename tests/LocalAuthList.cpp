// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_LOCAL_AUTH

#include <catch2/catch.hpp>

#include <MicroOcpp.h>
#include "./helpers/testHelper.h"

#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Model/Authorization/AuthorizationService.h>
#include <MicroOcpp/Model/Transactions/TransactionService16.h>


#define BASE_TIME_UNIX 1750000000
#define BASE_TIME_STRING "2025-06-15T15:06:40Z"

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

        char buf [MO_IDTAG_LEN_MAX + 1];
        sprintf(buf, "mIdTag%zu", i);
        authData[AUTHDATA_KEY_IDTAG(compact)] = buf;
        idTagInfo[AUTHDATA_KEY_STATUS(compact)] = "Accepted";
    }
}

TEST_CASE( "LocalAuth" ) {
    printf("\nRun %s\n",  "LocalAuth");

    //initialize Context without any configs
    mo_initialize();
    mo_useMockServer();

    mo_getContext()->setTicksCb(custom_timer_cb);

    mo_setOcppVersion(MO_OCPP_V16);

    mo_setBootNotificationData("TestModel", "TestVendor");

    mo_setup();

    auto authService = mo_getContext()->getModel16().getAuthorizationService();

    mo_setUnixTime(BASE_TIME_UNIX);

    loop();
    
    //enable local auth
    mo_setConfigurationBool("LocalAuthListEnabled", true);

    //set Authorize timeout after which the charger is considered offline
    const int32_t AUTH_TIMEOUT_SECS = 60;
    mo_setConfigurationInt(MO_CONFIG_EXT_PREFIX "AuthorizationTimeout", AUTH_TIMEOUT_SECS);

    SECTION("Clean up files") {
        auto filesystem = mo_getFilesystem();
        FilesystemUtils::removeByPrefix(filesystem, "");
    }

    SECTION("Basic local auth - LocalPreAuthorize") {

        mo_setConfigurationBool("LocalAuthorizeOffline", false);
        mo_setConfigurationBool("LocalPreAuthorize", true);
        
        //set local list
        StaticJsonDocument<256> localAuthList;
        localAuthList[0]["idTag"] = "mIdTag";
        localAuthList[0]["idTagInfo"]["status"] = "Accepted";

        REQUIRE( authService->updateLocalList(localAuthList.as<JsonArray>(), 1, false) );

        REQUIRE( authService->getLocalListSize() == 1 );
        REQUIRE( authService->getLocalAuthorization("mIdTag") != nullptr );
        REQUIRE( authService->getLocalAuthorization("mIdTag")->getAuthorizationStatus() == v16::AuthorizationStatus::Accepted );

        //begin transaction and delay Authorize request - tx should start immediately
        mo_loopback_setOnline(mo_getContext()->getConnection(), false); //Authorize delayed by short offline period

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Available );

        mo_beginTransaction("mIdTag");
        loop();

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Charging );
        REQUIRE( mo_isTransactionAuthorized() );

        mo_loopback_setOnline(mo_getContext()->getConnection(), true);
        mo_endTransaction(nullptr, nullptr);
        loop();

        //begin transaction delay Authorize request, but idTag doesn't match local list - tx should start when online again
        mo_loopback_setOnline(mo_getContext()->getConnection(), false);

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Available );

        mo_beginTransaction("wrong idTag");
        loop();

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Preparing );
        REQUIRE( !mo_isTransactionAuthorized() );

        mo_loopback_setOnline(mo_getContext()->getConnection(), true);
        loop();

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Charging );
        REQUIRE( mo_isTransactionAuthorized() );

        mo_endTransaction(nullptr, nullptr);
        loop();
    }

    SECTION("Basic local auth - LocalAuthorizeOffline") {

        mo_setConfigurationBool("LocalAuthorizeOffline", true);
        mo_setConfigurationBool("LocalPreAuthorize", false);

        //set local list
        StaticJsonDocument<256> localAuthList;
        localAuthList[0]["idTag"] = "mIdTag";
        localAuthList[0]["idTagInfo"]["status"] = "Accepted";
        authService->updateLocalList(localAuthList.as<JsonArray>(), 1, false);

        //make charger offline and begin tx - tx should begin after Authorize timeout
        mo_loopback_setOnline(mo_getContext()->getConnection(), false);

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Available );

        auto t_before = mo_getUptime();

        mo_beginTransaction("mIdTag");
        loop();

        REQUIRE( mo_getUptime() - t_before < AUTH_TIMEOUT_SECS ); //if this fails, increase AUTH_TIMEOUT_SECS
        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Preparing );
        REQUIRE( !mo_isTransactionAuthorized() );

        mtime += (unsigned long)(AUTH_TIMEOUT_SECS - (mo_getUptime() - t_before)) * 1000UL; //increment clock so that auth timeout is exceeded
        loop();

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Charging );
        REQUIRE( mo_isTransactionAuthorized() );

        mo_loopback_setOnline(mo_getContext()->getConnection(), true);
        mo_endTransaction(nullptr, nullptr);
        loop();

        //make charger offline and begin tx, but idTag doesn't match - tx should be aborted
        bool checkTxTimeout = false;
        mo_setTxNotificationOutput2(mo_getApiContext(), 1,
            [] (MO_TxNotification txNotification, unsigned int, void *userData) {
                if (txNotification == MO_TxNotification_AuthorizationTimeout) {
                    bool *checkTxTimeout = reinterpret_cast<bool*>(userData);
                    *checkTxTimeout = true;
                }
            }, reinterpret_cast<void*>(&checkTxTimeout));

        mo_loopback_setOnline(mo_getContext()->getConnection(), false);

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Available );

        t_before = mo_getUptime();

        mo_beginTransaction("wrong idTag");
        loop();

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Preparing );
        REQUIRE( !checkTxTimeout );

        mtime += (unsigned long)(AUTH_TIMEOUT_SECS - (mo_getUptime() - t_before)) * 1000UL; //increment clock so that auth timeout is exceeded
        loop();

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Available );
        REQUIRE( checkTxTimeout );

        mo_loopback_setOnline(mo_getContext()->getConnection(), true);
        loop();
    }

    SECTION("Basic local auth - AllowOfflineTxForUnknownId") {

        mo_setConfigurationBool("LocalAuthorizeOffline", false);
        mo_setConfigurationBool("LocalPreAuthorize", false);
        mo_setConfigurationBool("AllowOfflineTxForUnknownId", true);

        //make charger offline and begin tx - tx should begin after Authorize timeout
        mo_loopback_setOnline(mo_getContext()->getConnection(), false);

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Available );

        auto t_before = mo_getUptime();

        mo_beginTransaction("unknownIdTag");
        loop();

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Preparing );
        REQUIRE( !mo_isTransactionAuthorized() );

        mtime += (unsigned long)(AUTH_TIMEOUT_SECS - (mo_getUptime() - t_before)) * 1000UL; //increment clock so that auth timeout is exceeded
        loop();

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Charging );
        REQUIRE( mo_isTransactionAuthorized() );

        mo_loopback_setOnline(mo_getContext()->getConnection(), true);
        mo_endTransaction(nullptr, nullptr);
        loop();
    }

    SECTION("Local auth - check WS online status") {

        mo_setConfigurationBool("LocalAuthorizeOffline", false);
        mo_setConfigurationBool("LocalPreAuthorize", false);
        mo_setConfigurationBool("AllowOfflineTxForUnknownId", true);

        //disconnect WS and begin tx - charger should enter offline mode immediately
        mo_loopback_setConnected(mo_getContext()->getConnection(), false);

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Available );

        mo_beginTransaction("unknownIdTag");
        loop();

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Charging );
        REQUIRE( mo_isTransactionAuthorized() );

        mo_loopback_setConnected(mo_getContext()->getConnection(), true);
        mo_endTransaction(nullptr, nullptr);
        loop();
    }

    SECTION("Local auth list entry expired / unauthorized") {

        mo_setConfigurationBool("LocalPreAuthorize", true);

        //set local list with expired / unauthorized entry
        StaticJsonDocument<512> localAuthList;
        localAuthList[0]["idTag"] = "mIdTagExpired";
        localAuthList[0]["idTagInfo"]["status"] = "Accepted";
        localAuthList[0]["idTagInfo"]["expiryDate"] = BASE_TIME_STRING; //is in past
        localAuthList[1]["idTag"] = "mIdTagUnauthorized";
        localAuthList[1]["idTagInfo"]["status"] = "Blocked";
        authService->updateLocalList(localAuthList.as<JsonArray>(), 1, false);
        REQUIRE( authService->getLocalAuthorization("mIdTagExpired") );
        REQUIRE( authService->getLocalAuthorization("mIdTagUnauthorized") );

        REQUIRE( authService->getLocalAuthorization("mIdTagExpired") );

        //begin transaction and delay Authorize request - cannot PreAuthorize because entry is expired
        mo_loopback_setOnline(mo_getContext()->getConnection(), false); //Authorize delayed by short offline period

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Available );

        mo_beginTransaction("mIdTagExpired");
        loop();

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Preparing );

        mo_loopback_setOnline(mo_getContext()->getConnection(), true);
        loop();

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Charging );

        mo_endTransaction(nullptr, nullptr);
        loop();

        //begin transaction and delay Authorize request - cannot PreAuthorize because entry is unauthorized
        mo_loopback_setOnline(mo_getContext()->getConnection(), false); //Authorize delayed by short offline period

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Available );

        mo_beginTransaction("mIdTagUnauthorized");
        loop();

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Preparing );

        mo_loopback_setOnline(mo_getContext()->getConnection(), true);
        loop();

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Charging );
        mo_endTransaction(nullptr, nullptr);
        loop();
    }

    SECTION("Mix local authorization modes") {

        mo_setConfigurationBool("LocalAuthorizeOffline", true);
        mo_setConfigurationBool("LocalPreAuthorize", true);
        mo_setConfigurationBool("AllowOfflineTxForUnknownId", true);

        //set local list with accepted and accepted entry
        StaticJsonDocument<512> localAuthList;
        localAuthList[0]["idTag"] = "mIdTagExpired";
        localAuthList[0]["idTagInfo"]["status"] = "Accepted";
        localAuthList[0]["idTagInfo"]["expiryDate"] = BASE_TIME_STRING; //is in past
        localAuthList[1]["idTag"] = "mIdTagAccepted";
        localAuthList[1]["idTagInfo"]["status"] = "Accepted";
        authService->updateLocalList(localAuthList.as<JsonArray>(), 1, false);
        REQUIRE( authService->getLocalAuthorization("mIdTagExpired") );
        REQUIRE( authService->getLocalAuthorization("mIdTagAccepted") );

        //begin transaction and delay Authorize request - tx should start immediately
        mo_loopback_setOnline(mo_getContext()->getConnection(), false);

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Available );

        mo_beginTransaction("mIdTagAccepted");
        loop();

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Charging );

        mo_loopback_setOnline(mo_getContext()->getConnection(), true);
        mo_endTransaction(nullptr, nullptr);
        loop();

        //begin transaction, but idTag is expired - AllowOfflineTxForUnknownId must not apply
        mo_loopback_setOnline(mo_getContext()->getConnection(), false);

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Available );

        auto t_before = mo_getUptime();

        mo_beginTransaction("mIdTagExpired");
        loop();

        REQUIRE( mo_getUptime() - t_before < AUTH_TIMEOUT_SECS ); //if this fails, increase AUTH_TIMEOUT_SECS
        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Preparing );

        mtime += (unsigned long)(AUTH_TIMEOUT_SECS - (mo_getUptime() - t_before)) * 1000UL; //increment clock so that auth timeout is exceeded
        loop();

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Available );

        mo_loopback_setOnline(mo_getContext()->getConnection(), true);
        loop();
    }

    SECTION("DeAuthorize locally authorized tx") {

        mo_setConfigurationBool("LocalAuthorizeOffline", false);
        mo_setConfigurationBool("LocalPreAuthorize", true);

        //set local list
        StaticJsonDocument<256> localAuthList;
        localAuthList[0]["idTag"] = "mIdTag";
        localAuthList[0]["idTagInfo"]["status"] = "Accepted";
        authService->updateLocalList(localAuthList.as<JsonArray>(), 1, false);

        //patch Authorize so it will reject all idTags

        mo_getContext()->getMessageService().clearRegisteredOperation("Authorize");

        mo_getContext()->getMessageService().registerOperation("Authorize", nullptr,
            [] (const char *operationType, char *buf, size_t size, void *userStatus, void *userData) {
                (void)userStatus;
                (void)userData;
                return snprintf(buf, size, "{\"idTagInfo\":{\"status\":\"Blocked\"}}");
            });

        //begin transaction and delay Authorize request - tx should start immediately
        mo_loopback_setOnline(mo_getContext()->getConnection(), false); //Authorize delayed by short offline period

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Available );

        mo_beginTransaction("mIdTag");
        loop();

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Charging );
        REQUIRE( mo_isTransactionAuthorized() );

        //check TX notification
        bool checkTxRejected = false;
        mo_setTxNotificationOutput2(mo_getApiContext(), 1,
            [] (MO_TxNotification txNotification, unsigned int, void *userData) {
                if (txNotification == MO_TxNotification_AuthorizationRejected) {
                    bool *checkTxRejected = reinterpret_cast<bool*>(userData);
                    *checkTxRejected = true;
                }
            }, reinterpret_cast<void*>(&checkTxRejected));

        mo_loopback_setOnline(mo_getContext()->getConnection(), true);
        loop();

        REQUIRE( mo_getChargePointStatus() == MO_ChargePointStatus_Available );
        REQUIRE( checkTxRejected );

        loop();
    }

    SECTION("LocalListConflict") {

        mo_setConfigurationBool("LocalPreAuthorize", false);

        //patch Authorize so it will reject all idTags
        bool checkAuthorize = false;
        mo_getContext()->getMessageService().clearRegisteredOperation("Authorize");
        mo_getContext()->getMessageService().registerOperation("Authorize",
            [] (const char *operationType, const char *payloadJson, void **userStatus, void *userData) {
                bool *checkAuthorize = reinterpret_cast<bool*>(userData);
                *checkAuthorize = true;
            },
            [] (const char *operationType, char *buf, size_t size, void *userStatus, void *userData) {
                (void)userStatus;
                (void)userData;
                return snprintf(buf, size, "{\"idTagInfo\":{\"status\":\"Blocked\"}}");
            }, nullptr, reinterpret_cast<void*>(&checkAuthorize));
        
        //patch StartTransaction so it will DeAuthorize all txs
        bool checkStartTx = false;
        mo_getContext()->getMessageService().clearRegisteredOperation("StartTransaction");
        mo_getContext()->getMessageService().registerOperation("StartTransaction",
            [] (const char *operationType, const char *payloadJson, void **userStatus, void *userData) {
                bool *checkStartTx = reinterpret_cast<bool*>(userData);
                *checkStartTx = true;
            },
            [] (const char *operationType, char *buf, size_t size, void *userStatus, void *userData) {
                (void)userStatus;
                (void)userData;
                return snprintf(buf, size, "{\"idTagInfo\":{\"status\":\"Blocked\"}, \"transactionId\":1000}");
            }, nullptr, reinterpret_cast<void*>(&checkStartTx));
        
        //check resulting StatusNotification message
        bool checkLocalListConflict = false;
        mo_getContext()->getMessageService().clearRegisteredOperation("StatusNotification");
        mo_getContext()->getMessageService().registerOperation("StatusNotification",
            [] (const char *operationType, const char *payloadJson, void **userStatus, void *userData) {
                StaticJsonDocument<256> payload;
                deserializeJson(payload, payloadJson);
                if (payload["connectorId"] == 0 &&
                        !strcmp(payload["errorCode"] | "_Undefined", "LocalListConflict")) {
                    bool *checkLocalListConflict = reinterpret_cast<bool*>(userData);
                    *checkLocalListConflict = true;
                }
            },
            [] (const char *operationType, char *buf, size_t size, void *userStatus, void *userData) {
                (void)userStatus;
                (void)userData;
                return snprintf(buf, size, "{}");
            }, nullptr, reinterpret_cast<void*>(&checkLocalListConflict));

        //set local list
        StaticJsonDocument<256> localAuthList;
        localAuthList[0]["idTag"] = "mIdTag";
        localAuthList[0]["idTagInfo"]["status"] = "Accepted";
        authService->updateLocalList(localAuthList.as<JsonArray>(), 1, false);

        //send Authorize and StartTx and check if they trigger LocalListConflict
        mo_beginTransaction("mIdTag");
        loop();
        REQUIRE( checkLocalListConflict );
        REQUIRE( checkAuthorize );
        REQUIRE( !checkStartTx );

        checkLocalListConflict = false;
        checkAuthorize = false;
        mo_beginTransaction_authorized("mIdTag", nullptr);
        loop();
        REQUIRE( checkLocalListConflict );
        REQUIRE( !checkAuthorize );
        REQUIRE( checkStartTx );
    }

    SECTION("Update local list") {

        //reset list
        StaticJsonDocument<256> localAuthList;
        localAuthList.to<JsonArray>();
        authService->updateLocalList(localAuthList.as<JsonArray>(), 0, false);

        REQUIRE( authService->getLocalListSize() == 0 ); //idle, empty local list

        //set local list
        int localListVersion = 42;
        localAuthList.clear();
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
        REQUIRE( authService->getLocalAuthorization("mIdTag")->getAuthorizationStatus() == v16::AuthorizationStatus::Accepted );
        localListVersion++;
        localAuthList.clear();
        localAuthList[0]["idTag"] = "mIdTag";
        localAuthList[0]["idTagInfo"]["status"] = "Blocked";
        authService->updateLocalList(localAuthList.as<JsonArray>(), localListVersion, true);
        REQUIRE( authService->getLocalListVersion() == localListVersion );
        REQUIRE( authService->getLocalListSize() == 2 );
        REQUIRE( authService->getLocalAuthorization("mIdTag")->getAuthorizationStatus() == v16::AuthorizationStatus::Blocked );

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
        localAuthList[1]["idTagInfo"]["expiryDate"] = BASE_TIME_STRING;
        localAuthList[1]["idTagInfo"]["parentIdTag"] = "mParentIdTag";
        localAuthList[1]["idTagInfo"]["status"] = "Blocked";
        authService->updateLocalList(localAuthList.as<JsonArray>(), listVersion, false);

        mo_deinitialize();

        mo_initialize();
        mo_useMockServer();
        mo_getContext()->setTicksCb(custom_timer_cb);
        mo_setOcppVersion(MO_OCPP_V16);
        authService = mo_getContext()->getModel16().getAuthorizationService();
        mo_setup();

        REQUIRE( authService->getLocalListVersion() == listVersion );
        REQUIRE( authService->getLocalListSize() == 2 );
        auto auth0 = authService->getLocalAuthorization("mIdTagMinimal");
        REQUIRE( auth0 != nullptr );
        REQUIRE( !strcmp(auth0->getIdTag(), "mIdTagMinimal") );
        REQUIRE( auth0->getExpiryDate() == nullptr );
        REQUIRE( auth0->getParentIdTag() == nullptr );
        REQUIRE( auth0->getAuthorizationStatus() == v16::AuthorizationStatus::Accepted );
        
        auto auth1 = authService->getLocalAuthorization("mIdTagFull");
        REQUIRE( auth1 != nullptr );
        REQUIRE( !strcmp(auth1->getIdTag(), "mIdTagFull") );
        REQUIRE( auth1->getExpiryDate() != nullptr );
        Timestamp baseTimeParsed;
        mo_getContext()->getClock().fromUnixTime(baseTimeParsed, BASE_TIME_UNIX);
        int32_t dt = -1;
        REQUIRE( mo_getContext()->getClock().delta(*auth1->getExpiryDate(), baseTimeParsed, dt) );
        REQUIRE( dt == 0 );
        REQUIRE( !strcmp(auth1->getParentIdTag(), "mParentIdTag") );
        REQUIRE( auth1->getAuthorizationStatus() == v16::AuthorizationStatus::Blocked );
    }

#if 0

    SECTION("SendLocalList") {

        int listVersion = 42;
        size_t listSize = 2;
        auto populatedEntryIdTag = makeString("UnitTests"); //local auth list entry to be fully populated
        
        //Full update - happy path
        bool checkAccepted = false;
        getOcppContext()->initiateRequest(makeRequest(
            new v16::CustomOperation("SendLocalList",
                [&listVersion, &listSize, &populatedEntryIdTag] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", 
                            4096);
                    auto payload = doc->to<JsonObject>();
                    payload["listVersion"] = listVersion;
                    generateAuthList(payload["localAuthorizationList"].to<JsonArray>(), listSize, false);

                    //fully populate first entry
                    populatedEntryIdTag = payload["localAuthorizationList"][0]["idTag"] | "_Undefined";
                    payload["localAuthorizationList"][0]["idTagInfo"]["expiryDate"] = BASE_TIME_STRING;
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
        baseTimeParsed.setTime(BASE_TIME_STRING);
        REQUIRE( localAuth->getExpiryDate() );
        REQUIRE( *localAuth->getExpiryDate() == baseTimeParsed );
        REQUIRE( !strcmp(localAuth->getIdTag(), populatedEntryIdTag.c_str()) );
        REQUIRE( !strcmp(localAuth->getParentIdTag(), "mParentIdTag") );
        REQUIRE( localAuth->getAuthorizationStatus() == v16::AuthorizationStatus::Blocked );
        REQUIRE( checkAccepted );

        //Differential update - happy path
        listVersion++;
        listSize++; //add one extra entry and update all others

        checkAccepted = false;
        getOcppContext()->initiateRequest(makeRequest(
            new v16::CustomOperation("SendLocalList",
                [&listVersion, &listSize] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", 
                            1024);
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
            new v16::CustomOperation("SendLocalList",
                [&listVersion, &listSizeInvalid] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", 
                            1024);
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
        int listSizeInt;
        mo_getConfigurationInt("SendLocalListMaxLength", &listSizeInt);
        listSize = (size_t)listSizeInt;

        checkAccepted = false;
        getOcppContext()->initiateRequest(makeRequest(
            new v16::CustomOperation("SendLocalList",
                [&listVersion, &listSize] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", 
                            4096);
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
            new v16::CustomOperation("SendLocalList",
                [&listVersion, &listSize] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", 
                            4096);
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
            new v16::CustomOperation("SendLocalList",
                [&listVersionInvalid, &listSizeInvalid] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", 
                            4096);
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

        int localAuthListMaxLengthInt;
        mo_getConfigurationInt("LocalAuthListMaxLength", &localAuthListMaxLengthInt);
        localAuthListMaxLength = (size_t)localAuthListMaxLengthInt;

        //send Differential lists with 2 entries: update an existing entry and add a new entry
        for (size_t i = 1; i < localAuthListMaxLength; i++) {
            //Full update - happy path
            bool checkAccepted = false;
            getOcppContext()->initiateRequest(makeRequest(
                new v16::CustomOperation("SendLocalList",
                    [&i] () {
                        //create req
                        auto doc = makeJsonDoc("UnitTests", 
                                1024);
                        auto payload = doc->to<JsonObject>();
                        payload["listVersion"] = (int) i;

                        char buf [MO_IDTAG_LEN_MAX + 1];
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
            REQUIRE( authService->getLocalListVersion() == (int)i );
            REQUIRE( authService->getLocalListSize() == i + 1 );
            REQUIRE( checkAccepted );
        }

        //now exceed local list by sending overflow entry
        listVersion = authService->getLocalListVersion();
        listVersionInvalid = listVersion + 1;
        listSize = authService->getLocalListSize();

        bool checkFailed = false;
        getOcppContext()->initiateRequest(makeRequest(
            new v16::CustomOperation("SendLocalList",
                [&listVersionInvalid] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", 
                            1024);
                    auto payload = doc->to<JsonObject>();
                    payload["listVersion"] = listVersionInvalid;

                    //update already existing entry
                    char buf [MO_IDTAG_LEN_MAX + 1];
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
            new v16::CustomOperation("GetLocalListVersion",
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

#endif

    mo_deinitialize();
}

#endif //MO_ENABLE_LOCAL_AUTH
