// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Operations/CustomOperation.h>
#include <catch2/catch.hpp>
#include "./helpers/testHelper.h"

#include <vector>

using namespace MicroOcpp;
using namespace MicroOcpp::v201;

#define KNOWN_KEY "__ExistingKey"
#define UNKOWN_KEY "__UnknownKey"

// Storage for captured operation responses
std::vector<std::pair<std::string, DynamicJsonDocument>> capturedOperations;

void handleVariableRequest(const char *operationType, const char *payloadJson, void*) {
    DynamicJsonDocument doc(2048);
    auto err = deserializeJson(doc, payloadJson);
    if (err) {
        char buf[100];
        snprintf(buf, sizeof(buf), "JSON deserialization error: %s", err.c_str());
        FAIL(buf);
    }
    capturedOperations.emplace_back(operationType, std::move(doc));
}

// Global variables for NotifyReport test
bool *g_checkProcessedNotification = nullptr;
Timestamp *g_checkTimestamp = nullptr;
int *g_checkSeqNo = nullptr;
bool *g_checkFoundVar = nullptr;

void handleNotifyReportRequest(const char *operationType, const char *payloadJson, void **userStatus, void*) {
    if (strcmp(operationType, "NotifyReport") == 0) {
        DynamicJsonDocument doc(8192);
        auto err = deserializeJson(doc, payloadJson);
        if (err) {
            char buf[100];
            snprintf(buf, sizeof(buf), "JSON deserialization error: %s", err.c_str());
            FAIL(buf);
        }

        // Process NotifyReport request
        if (g_checkProcessedNotification) {
            *g_checkProcessedNotification = true;
        }
        REQUIRE((doc["requestId"] | -1) == 1);
        if (g_checkTimestamp) {
            mo_getContext()->getClock().parseString(doc["generatedAt"] | "_Undefined", *g_checkTimestamp);
        }
        if (g_checkSeqNo) {
            REQUIRE((doc["seqNo"] | -1) == *g_checkSeqNo + 1); // Check sequentiality
            *g_checkSeqNo = doc["seqNo"] | -1;
        }

        for (auto reportData : doc["reportData"].as<JsonArray>()) {
            if (!strcmp(reportData["component"]["name"] | "_Undefined", "mComponent") &&
                    !strcmp(reportData["variable"]["name"] | "_Undefined", "mString")) {
                if (g_checkFoundVar) {
                    *g_checkFoundVar = true;
                }
            }
        }
    }
}

int writeNotifyReportResponse(const char *operationType, char *buf, size_t size, void*, void*) {
    return snprintf(buf, size, "{}");
}

TEST_CASE("Variable") {
    printf("\nRun %s\n", "Variable");

    capturedOperations.clear();

    // Initialize Context
    mo_initialize();
    mo_useMockServer();

    // Set platform configurations (before mo_setup)
    mo_setTicksCb(custom_timer_cb);

    mo_setOcppVersion(MO_OCPP_V201);
    mo_setBootNotificationData("TestModel", "TestVendor");

    // Capture variable operation messages (before mo_setup)
    mo_setOnReceiveRequest(mo_getApiContext(), "GetVariables",
                        handleVariableRequest, nullptr);
    mo_setOnReceiveRequest(mo_getApiContext(), "SetVariables",
                        handleVariableRequest, nullptr);
    mo_setOnReceiveRequest(mo_getApiContext(), "GetBaseReport",
                        handleVariableRequest, nullptr);
    mo_setRequestHandler(mo_getApiContext(), "NotifyReport",
                        handleNotifyReportRequest, writeNotifyReportResponse, nullptr, nullptr);

    // Finalize setup
    mo_setup();

    SECTION("Clean up files") {
        auto filesystem = mo_getFilesystem();
        if (filesystem) {
            FilesystemUtils::removeByPrefix(filesystem, "");
        }
    }

    SECTION("Basic container operations") {
        auto vs = mo_getContext()->getModel201().getVariableService();
        REQUIRE(vs != nullptr);

        // Test variable declaration and retrieval
        auto varInt = vs->declareVariable<int>("mComponent", "cInt", 42);
        REQUIRE(varInt != nullptr);
        REQUIRE(varInt->getInt() == 42);

        auto varBool = vs->declareVariable<bool>("mComponent", "cBool", true);
        REQUIRE(varBool != nullptr);
        REQUIRE(varBool->getBool() == true);

        auto varString = vs->declareVariable<const char*>("mComponent", "cString", "mValue");
        REQUIRE(varString != nullptr);
        REQUIRE(strcmp(varString->getString(), "mValue") == 0);

        // Test variable retrieval by component and name
        auto retrievedInt = vs->getVariable("mComponent", "cInt");
        REQUIRE(retrievedInt == varInt);

        auto retrievedBool = vs->getVariable("mComponent", "cBool");
        REQUIRE(retrievedBool == varBool);

        auto retrievedString = vs->getVariable("mComponent", "cString");
        REQUIRE(retrievedString == varString);

        // Test non-existent variable
        auto nonExistent = vs->getVariable("mComponent", "nonExistent");
        REQUIRE(nonExistent == nullptr);
    }

    SECTION("Persistency on filesystem") {
        auto filesystem = mo_getFilesystem();
        REQUIRE(filesystem != nullptr);

        // Clean up any existing files
        FilesystemUtils::removeByPrefix(filesystem, "");

        auto vs = mo_getContext()->getModel201().getVariableService();
        REQUIRE(vs != nullptr);

        // Declare a persistent variable
        auto varString = vs->declareVariable<const char*>("mComponent", "persistentString", "initialValue",
                                                         Mutability::ReadWrite, true); // persistent = true
        REQUIRE(varString != nullptr);
        REQUIRE(strcmp(varString->getString(), "initialValue") == 0);

        // Modify the variable value
        varString->setString("modifiedValue");
        REQUIRE(strcmp(varString->getString(), "modifiedValue") == 0);
        REQUIRE(vs->commit());

        // Simulate restart by deinitializing and reinitializing
        mo_deinitialize();

        // Reinitialize with correct order
        mo_initialize();
        mo_useMockServer();

        // Set platform configurations (before mo_setup)
        mo_setTicksCb(custom_timer_cb);
        mo_setOcppVersion(MO_OCPP_V201);
        mo_setBootNotificationData("TestModel", "TestVendor");

        // Re-declare the same variable - should load persisted value
        auto vs2 = mo_getContext()->getModel201().getVariableService();
        auto varString2 = vs2->declareVariable<const char*>("mComponent", "persistentString", "defaultValue",
                                                           Mutability::ReadWrite, true);
        REQUIRE(varString2 != nullptr);

        // Finalize setup
        mo_setup();

        // Should have loaded the persisted value, not the default
        REQUIRE(strcmp(varString2->getString(), "modifiedValue") == 0);
    }

    SECTION("Variable API") {
        auto vs = mo_getContext()->getModel201().getVariableService();
        REQUIRE(vs != nullptr);

        // Declare variables
        auto cInt = vs->declareVariable<int>("mComponent", "cInt", 42);
        REQUIRE(cInt != nullptr);
        REQUIRE(cInt->getInt() == 42);

        auto cBool = vs->declareVariable<bool>("mComponent", "cBool", true);
        REQUIRE(cBool != nullptr);
        REQUIRE(cBool->getBool() == true);

        auto cString = vs->declareVariable<const char*>("mComponent", "cString", "mValue");
        REQUIRE(cString != nullptr);
        REQUIRE(strcmp(cString->getString(), "mValue") == 0);

        // Fetch variable by re-declaring (should return same instance)
        auto cInt2 = vs->declareVariable<int>("mComponent", "cInt", -1);
        REQUIRE(cInt2 == cInt);
        REQUIRE(cInt2->getInt() == 42); // Should keep original value, not default

        // Declare variable twice - should return same instance
        auto cInt3 = vs->declareVariable<int>("mComponent", "cInt", -1);
        REQUIRE(cInt3 == cInt2);

        // Test variable accessibility and permissions
        Mutability mutability = Mutability::ReadWrite;
        bool persistent = false;
        Variable::AttributeTypeSet attrs = Variable::AttributeType::Actual;
        bool rebootRequired = false;

        auto cInt6 = vs->declareVariable<int>("mComponent", "cIntPerms", 42, mutability, persistent, attrs, rebootRequired);
        REQUIRE(cInt6 != nullptr);
        REQUIRE(cInt6->getMutability() == Mutability::ReadWrite);
        REQUIRE(!cInt6->isPersistent());
        REQUIRE(!cInt6->isRebootRequired());

        // Revoke permissions - make it read-only, persistent, and reboot required
        mutability = Mutability::ReadOnly;
        persistent = true;
        rebootRequired = true;
        auto cInt7 = vs->declareVariable<int>("mComponent", "cIntPerms", 42, mutability, persistent, attrs, rebootRequired);
        REQUIRE(cInt7 == cInt6); // Should be same instance
        REQUIRE(cInt6->getMutability() == Mutability::ReadOnly);
        REQUIRE(cInt6->isPersistent());
        REQUIRE(cInt6->isRebootRequired());

        // Try to revert permissions - should not be possible (once restricted, always restricted)
        mutability = Mutability::ReadWrite;
        persistent = false;
        rebootRequired = false;
        auto cInt8 = vs->declareVariable<int>("mComponent", "cIntPerms", 42, mutability, persistent, attrs, rebootRequired);
        REQUIRE(cInt8 == cInt6); // Should be same instance
        REQUIRE(cInt8->getMutability() == Mutability::ReadOnly); // Should remain read-only
        REQUIRE(cInt8->isPersistent()); // Should remain persistent
        REQUIRE(cInt8->isRebootRequired()); // Should remain reboot required
    }

#if 0
    SECTION("Main lib integration") {

        //basic lifecycle
        mocpp_initialize(loopback, ChargerCredentials(), filesystem, false, ProtocolVersion(2,0,1));
        REQUIRE( getVariablePublic("ConnectionTimeOut") );
        REQUIRE( !getVariableContainersPublic().empty() );
        mocpp_deinitialize();
        REQUIRE( !getVariablePublic("ConnectionTimeOut") );
        REQUIRE( getVariableContainersPublic().empty() );

        //modify standard config ConnectionTimeOut. This config is not modified by the main lib during normal initialization / deinitialization
        mocpp_initialize(loopback, ChargerCredentials(), filesystem, false, ProtocolVersion(2,0,1));
        auto config = getVariablePublic("ConnectionTimeOut");

        config->setInt(1234); //update
        configuration_save(); //write back

        mocpp_deinitialize();

        mocpp_initialize(loopback, ChargerCredentials(), filesystem, false, ProtocolVersion(2,0,1));
        REQUIRE( getVariablePublic("ConnectionTimeOut")->getInt() == 1234 );

        mocpp_deinitialize();
    }
#endif

#if 0
    SECTION("GetVariables") {

        mocpp_initialize(loopback, ChargerCredentials(), filesystem, false, ProtocolVersion(2,0,1));
        loop();

        vs->declareVariable<int>(KNOWN_KEY, 1234, MO_FILENAME_PREFIX "persistent1.jsn", false);

        bool checkProcessed = false;

        getOcppContext()->initiateRequest(makeRequest(new v16::CustomOperation(
                "GetVariables",
                [] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(1));
                    auto payload = doc->to<JsonObject>();
                    return doc;},
                [&checkProcessed] (JsonObject payload) {
                    //receive conf
                    checkProcessed = true;

                    JsonArray configurationKey = payload["configurationKey"];

                    bool foundCustomConfig = false;
                    bool foundStandardConfig = false;
                    for (JsonObject keyvalue : configurationKey) {
                        MO_DBG_DEBUG("key %s", keyvalue["key"] | "_Undefined");
                        if (!strcmp(keyvalue["key"] | "_Undefined", KNOWN_KEY)) {
                            foundCustomConfig = true;
                            REQUIRE( (keyvalue["readonly"] | true) == false );
                            REQUIRE( !strcmp(keyvalue["value"] | "_Undefined", "1234") );
                        } else if (!strcmp(keyvalue["key"] | "_Undefined", "ConnectionTimeOut")) {
                            foundStandardConfig = true;
                        }
                    }

                    REQUIRE( foundCustomConfig );
                    REQUIRE( foundStandardConfig );
                }
        )));

        loop();

        REQUIRE(checkProcessed);

        checkProcessed = false;

        getOcppContext()->initiateRequest(makeRequest(new v16::CustomOperation(
                "GetVariable",
                [] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(1) + JSON_ARRAY_SIZE(2));
                    auto payload = doc->to<JsonObject>();
                    auto key = payload.createNestedArray("key");
                    key.add(KNOWN_KEY);
                    key.add(UNKOWN_KEY);
                    return doc;},
                [&checkProcessed] (JsonObject payload) {
                    //receive conf
                    checkProcessed = true;

                    JsonArray configurationKey = payload["configurationKey"];

                    bool foundCustomConfig = false;
                    for (JsonObject keyvalue : configurationKey) {
                        if (!strcmp(keyvalue["key"] | "_Undefined", KNOWN_KEY)) {
                            foundCustomConfig = true;
                            break;
                        }
                    }
                    REQUIRE( foundCustomConfig );

                    JsonArray unknownKey = payload["unknownKey"];

                    bool foundUnkownKey = false;
                    for (const char *key : unknownKey) {
                        if (!strcmp(key, UNKOWN_KEY)) {
                            foundUnkownKey = true;
                        }
                    }

                    REQUIRE( foundUnkownKey );
                }
        )));

        loop();

        REQUIRE(checkProcessed);

        mocpp_deinitialize();
    }

    SECTION("ChangeVariable") {

        mocpp_initialize(loopback, ChargerCredentials(), filesystem, false, ProtocolVersion(2,0,1));
        loop();

        vs->declareVariable<int>(KNOWN_KEY, 0, MO_FILENAME_PREFIX "persistent1.jsn", false);

        //update existing config
        bool checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new v16::CustomOperation(
                "ChangeVariable",
                [] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(2));
                    auto payload = doc->to<JsonObject>();
                    payload["key"] = KNOWN_KEY;
                    payload["value"] = "1234";
                    return doc;},
                [&checkProcessed] (JsonObject payload) {
                    //receive conf
                    checkProcessed = true;

                    REQUIRE( !strcmp(payload["status"] | "_Undefined", "Accepted") );
                }
        )));
        loop();
        REQUIRE(checkProcessed);
        REQUIRE( getVariablePublic(KNOWN_KEY)->getInt() == 1234 );

        //try to update not existing key
        checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new v16::CustomOperation(
                "ChangeVariable",
                [] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(2));
                    auto payload = doc->to<JsonObject>();
                    payload["key"] = UNKOWN_KEY;
                    payload["value"] = "no effect";
                    return doc;},
                [&checkProcessed] (JsonObject payload) {
                    //receive conf
                    checkProcessed = true;

                    REQUIRE( !strcmp(payload["status"] | "_Undefined", "NotSupported") );
                }
        )));
        loop();
        REQUIRE( checkProcessed );

        //try to update config with malformatted value
        checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new v16::CustomOperation(
                "ChangeVariable",
                [] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(2));
                    auto payload = doc->to<JsonObject>();
                    payload["key"] = KNOWN_KEY;
                    payload["value"] = "not convertible to int";
                    return doc;},
                [&checkProcessed] (JsonObject payload) {
                    //receive conf
                    checkProcessed = true;

                    REQUIRE( !strcmp(payload["status"] | "_Undefined", "Rejected") );
                }
        )));
        loop();
        REQUIRE( checkProcessed );

        //try to update config with value validation
        //value is valid if it begins with 1
        registerVariableValidator(KNOWN_KEY, [] (const char *v) {
            return v[0] == '1';
        });

        //validation success
        checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new v16::CustomOperation(
                "ChangeVariable",
                [] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(2));
                    auto payload = doc->to<JsonObject>();
                    payload["key"] = KNOWN_KEY;
                    payload["value"] = "100234";
                    return doc;},
                [&checkProcessed] (JsonObject payload) {
                    //receive conf
                    checkProcessed = true;

                    REQUIRE( !strcmp(payload["status"] | "_Undefined", "Accepted") );
                }
        )));
        loop();
        REQUIRE( checkProcessed );
        REQUIRE( getVariablePublic(KNOWN_KEY)->getInt() == 100234 );

        //validation failure
        checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new v16::CustomOperation(
                "ChangeVariable",
                [] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(2));
                    auto payload = doc->to<JsonObject>();
                    payload["key"] = KNOWN_KEY;
                    payload["value"] = "4321";
                    return doc;},
                [&checkProcessed] (JsonObject payload) {
                    //receive conf
                    checkProcessed = true;

                    REQUIRE( !strcmp(payload["status"] | "_Undefined", "Rejected") );
                }
        )));
        loop();
        REQUIRE( checkProcessed );
        REQUIRE( getVariablePublic(KNOWN_KEY)->getInt() == 100234 ); //keep old value

        mocpp_deinitialize();
    }

    SECTION("Define factory defaults for standard configs") {

        //set factory default for standard config ConnectionTimeOut
        configuration_init(filesystem);
        auto factoryConnectionTimeOut = vs->declareVariable<int>("ConnectionTimeOut", 1234, MO_FILENAME_PREFIX "factory.jsn");

        mocpp_initialize(loopback, ChargerCredentials(), filesystem, false, ProtocolVersion(2,0,1));

        auto connectionTimeout2 = vs->declareVariable<int>("ConnectionTimeOut", 4321);
        REQUIRE( connectionTimeout2->getInt() == 1234 );
        REQUIRE( connectionTimeout2 == factoryConnectionTimeOut );

        configuration_save();
        mocpp_deinitialize();

        //this time, factory default is not given (will lead to duplicates, should be considered in sanitization)
        mocpp_initialize(loopback, ChargerCredentials(), filesystem, false, ProtocolVersion(2,0,1));
        REQUIRE( getVariablePublic("ConnectionTimeOut")->getInt() != 1234 );
        mocpp_deinitialize();

        //provide factory default again
        configuration_init(filesystem);
        vs->declareVariable<int>("ConnectionTimeOut", 4321, MO_FILENAME_PREFIX "factory.jsn");
        mocpp_initialize(loopback, ChargerCredentials(), filesystem, false, ProtocolVersion(2,0,1));
        REQUIRE( getVariablePublic("ConnectionTimeOut")->getInt() == 1234 );
        mocpp_deinitialize();

    }
#endif

    SECTION("GetVariables request") {
        auto vs = mo_getContext()->getModel201().getVariableService();
        REQUIRE(vs != nullptr);

        // Declare a test variable
        auto varString = vs->declareVariable<const char*>("mComponent", "mString", "mValue");
        REQUIRE(varString != nullptr);
        REQUIRE(strcmp(varString->getString(), "mValue") == 0);

        loop();

        capturedOperations.clear();
        bool checkProcessed = false;

        // Create and send GetVariables request
        mo_sendRequest(mo_getApiContext(),
            "GetVariables",
            "{"
                "\"getVariableData\": ["
                    "{"
                        "\"component\": {\"name\": \"mComponent\"}, "
                        "\"variable\": {\"name\": \"mString\"}"
                    "}"
                "]"
            "}",
            [] (const char *payloadJson, void *userData) {
                auto checkProcessed = static_cast<bool*>(userData);
                *checkProcessed = true;
                StaticJsonDocument<512> doc;
                deserializeJson(doc, payloadJson);
                JsonArray getVariableResult = doc["getVariableResult"];
                REQUIRE(!strcmp(getVariableResult[0]["attributeStatus"] | "_Undefined", "Accepted"));
                REQUIRE(!strcmp(getVariableResult[0]["component"]["name"] | "_Undefined", "mComponent"));
                REQUIRE(!strcmp(getVariableResult[0]["variable"]["name"] | "_Undefined", "mString"));
                REQUIRE(!strcmp(getVariableResult[0]["attributeValue"] | "_Undefined", "mValue"));
            }, nullptr, static_cast<void*>(&checkProcessed));

        loop();

        REQUIRE(checkProcessed);
    }

    SECTION("SetVariables request") {
        auto vs = mo_getContext()->getModel201().getVariableService();
        REQUIRE(vs != nullptr);

        // Declare a test variable with empty initial value
        auto varString = vs->declareVariable<const char*>("mComponent", "mString", "");
        REQUIRE(varString != nullptr);
        REQUIRE(strcmp(varString->getString(), "") == 0);

        loop();

        capturedOperations.clear();
        bool checkProcessed = false;

        // Create and send SetVariables request
        mo_sendRequest(mo_getApiContext(),
            "SetVariables",
            "{"
                "\"setVariableData\": ["
                    "{"
                        "\"component\": {\"name\": \"mComponent\"}, "
                        "\"variable\": {\"name\": \"mString\"}, "
                        "\"attributeValue\": \"mValue\""
                    "}"
                "]"
            "}",
            [] (const char *payloadJson, void *userData) {
                auto checkProcessed = static_cast<bool*>(userData);
                *checkProcessed = true;
                StaticJsonDocument<512> doc;
                deserializeJson(doc, payloadJson);
                JsonArray setVariableResult = doc["setVariableResult"];
                REQUIRE(!strcmp(setVariableResult[0]["attributeStatus"] | "_Undefined", "Accepted"));
                REQUIRE(!strcmp(setVariableResult[0]["component"]["name"] | "_Undefined", "mComponent"));
                REQUIRE(!strcmp(setVariableResult[0]["variable"]["name"] | "_Undefined", "mString"));
            }, nullptr, static_cast<void*>(&checkProcessed));

        loop();

        REQUIRE(checkProcessed);

        // Verify the variable value was actually changed
        REQUIRE(strcmp(varString->getString(), "mValue") == 0);
    }

    SECTION("GetBaseReport request") {
        auto vs = mo_getContext()->getModel201().getVariableService();
        REQUIRE(vs != nullptr);

        // Declare a test variable
        auto varString = vs->declareVariable<const char*>("mComponent", "mString", "testValue");
        REQUIRE(varString != nullptr);
        REQUIRE(strcmp(varString->getString(), "testValue") == 0);

        loop();

        capturedOperations.clear();
        bool checkProcessedNotification = false;
        bool checkProcessed = false;
        Timestamp checkTimestamp;
        int checkSeqNo = -1;
        bool checkFoundVar = false;

        // Set up global variables for NotifyReport handler
        g_checkProcessedNotification = &checkProcessedNotification;
        g_checkTimestamp = &checkTimestamp;
        g_checkSeqNo = &checkSeqNo;
        g_checkFoundVar = &checkFoundVar;

        // Create and send GetBaseReport request
        mo_sendRequest(mo_getApiContext(),
            "GetBaseReport",
            "{"
                "\"requestId\": 1,"
                "\"reportBase\": \"FullInventory\""
            "}",
            [] (const char *payloadJson, void *userData) {
                auto checkProcessed = static_cast<bool*>(userData);
                *checkProcessed = true;
                StaticJsonDocument<256> doc;
                deserializeJson(doc, payloadJson);
                REQUIRE(!strcmp(doc["status"] | "_Undefined", "Accepted"));
            }, nullptr, static_cast<void*>(&checkProcessed));

        loop();

        REQUIRE(checkProcessed);
        REQUIRE(checkProcessedNotification);
        REQUIRE(checkSeqNo >= 0);
        REQUIRE(checkFoundVar);
        int32_t dt = 0;
        mo_getContext()->getClock().delta(mo_getContext()->getClock().now(), checkTimestamp, dt);
        REQUIRE(std::abs(dt) <= 10);

        // Clean up global variables
        g_checkProcessedNotification = nullptr;
        g_checkTimestamp = nullptr;
        g_checkSeqNo = nullptr;
        g_checkFoundVar = nullptr;
    }

    mo_deinitialize();
}

#endif //MO_ENABLE_V201
