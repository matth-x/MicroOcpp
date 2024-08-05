// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include "./catch2/catch.hpp"
#include "./helpers/testHelper.h"

#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Model/Variables/VariableService.h>

#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Operations/CustomOperation.h>

using namespace MicroOcpp;

#define GET_CONFIG_ALL "[2,\"test-msg\",\"GetVariable\",{}]"
#define KNOWN_KEY "__ExistingKey"
#define UNKOWN_KEY "__UnknownKey"
#define GET_CONFIG_KNOWN_UNKOWN "[2,\"test-mst\",\"GetVariable\",{\"key\":[\"" KNOWN_KEY "\",\"" UNKOWN_KEY "\"]}]"

TEST_CASE( "Variable" ) {
    printf("\nRun %s\n",  "Variable");

    //clean state
    auto filesystem = makeDefaultFilesystemAdapter(FilesystemOpt::Use_Mount_FormatOnFail);
    FilesystemUtils::remove_if(filesystem, [] (const char*) {return true;});

    SECTION("Basic container operations"){
        std::unique_ptr<VariableContainer> container;

        SECTION("Volatile storage") {
            container = makeVariableContainerVolatile(MO_VARIABLE_VOLATILE "/volatile1", true);
        }

#if 0
        SECTION("Persistent storage") {
            container = makeVariableContainerFlash(filesystem, MO_FILENAME_PREFIX "persistent1.jsn", true);
        }
#endif

        //check emptyness
        REQUIRE( container->size() == 0 );

        //add first config, fetch by index
        Variable::AttributeTypeSet attrs = Variable::AttributeType::Actual;
        auto configFirst = container->createVariable(Variable::InternalDataType::Int, attrs);
        configFirst->setName("cFirst");
        configFirst->setComponentId("mComponent");
        auto configFirstRaw = configFirst.get();
        REQUIRE( container->size() == 0 );
        REQUIRE( container->add(std::move(configFirst)) );
        REQUIRE( container->size() == 1 );
        REQUIRE( container->getVariable((size_t) 0) == configFirstRaw);

        //add one config of each type
        auto cInt = container->createVariable(Variable::InternalDataType::Int, attrs);
        cInt->setName("cInt");
        cInt->setComponentId("mComponent");
        auto cBool = container->createVariable(Variable::InternalDataType::Bool, attrs);
        cBool->setName("cBool");
        cBool->setComponentId("mComponent");
        auto cBoolRaw = cBool.get();
        auto cString = container->createVariable(Variable::InternalDataType::String, attrs);
        cString->setName("cString");
        cString->setComponentId("mComponent");

        container->add(std::move(cInt));
        container->add(std::move(cBool));
        container->add(std::move(cString));

        REQUIRE( container->size() == 4 );

        //fetch config by key
        REQUIRE( container->getVariable(cBoolRaw->getComponentId(), cBoolRaw->getName()) == cBoolRaw);
    }

#if 0
    SECTION("Persistency on filesystem") {

        auto container = makeVariableContainerFlash(filesystem, MO_FILENAME_PREFIX "persistent1.jsn", true);

        //trivial load call
        REQUIRE( container->load() );
        REQUIRE( container->size() == 0 );

        //add config, store, load again
        auto cString = container->createVariable(Variable::InternalDataType::String, "cString");
        cString->setString("mValue");

        REQUIRE( container->save() ); //store

        container.reset(); //destroy

        //...load again
        auto container2 = makeVariableContainerFlash(filesystem, MO_FILENAME_PREFIX "persistent1.jsn", true);
        REQUIRE( container2->size() == 0 );
        REQUIRE( container2->load() );
        REQUIRE( container2->size() == 1 );

        auto cString2 = container2->getVariable("cString");
        REQUIRE( cString2 != nullptr );
        REQUIRE( !strcmp(cString2->getString(), "mValue") );
    }
#endif

    LoopbackConnection loopback; //initialize Context with dummy socket
    mocpp_set_timer(custom_timer_cb);

    SECTION("Variable API") {

        //declare configs
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        auto vs = getOcppContext()->getModel().getVariableService();
        auto cInt = vs->declareVariable<int>("mComponent", "cInt", 42);
        REQUIRE( cInt != nullptr );
        vs->declareVariable<bool>("mComponent", "cBool", true);
        vs->declareVariable<const char*>("mComponent", "cString", "mValue");

        //fetch config
        REQUIRE( vs->declareVariable("mComponent", "cInt", -1)->getInt() == 42 );

#if 0
        //store, destroy, reload
        REQUIRE( configuration_save() );
        cInt.reset();
        configuration_deinit();
        REQUIRE( getVariablePublic("cInt") == nullptr);

        REQUIRE( configuration_init(filesystem) ); //reload

        //fetch configs (declare with different factory default - should remain at original value)
        auto cInt2 = vs->declareVariable<int>("cInt", -1);
        auto cBool2 = vs->declareVariable<bool>("cBool", false);
        auto cString2 = vs->declareVariable<const char*>("cString", "no effect");
        REQUIRE( configuration_load() ); //load config objects with stored values

        //check load result
        REQUIRE( cInt2->getInt() == 42 );
        REQUIRE( cBool2->getBool() == true );
        REQUIRE( !strcmp(cString2->getString(), "mValue") );
#else
        auto cInt2 = cInt;
#endif

        //declare config twice
        auto cInt3 = vs->declareVariable<int>("mComponent", "cInt", -1);
        REQUIRE( cInt3 == cInt2 );

        //declare config twice but in different container
        auto cInt4 = vs->declareVariable<int>("mComponent", "cInt", -1, MO_VARIABLE_VOLATILE "/volatile2");
        REQUIRE( cInt4 == cInt2 );

        //declare config twice but with conflicting type (will supersede old type because to simplify FW upgrades)
        auto cNewType = vs->declareVariable<const char*>("mComponent", "cInt", "mValue2");
        REQUIRE( cNewType != cInt2 );
        REQUIRE( cInt2->isDetached() ); // Container should not store this
        REQUIRE( !strcmp(cNewType->getString(), "mValue2") );

#if 0
        //store, destroy, reload
        REQUIRE( configuration_save() );
        configuration_deinit();
        REQUIRE( getVariablePublic("cInt") == nullptr);
        REQUIRE( configuration_init(filesystem) ); //reload
        auto cNewType2 = vs->declareVariable<const char*>("cInt", "no effect");
        REQUIRE( configuration_load() );
        REQUIRE( !strcmp(cNewType2->getString(), "mValue2") );

        //get config before declared (container needs to be declared already at this point)
        auto cString3 = getVariablePublic("cString");
        REQUIRE( !strcmp(cString3->getString(), "mValue") );
        configuration_deinit();

        //value needs to outlive container
        configuration_init(filesystem);
        auto cString4 = vs->declareVariable<const char *>("cString2", "mValue3");
        configuration_deinit();
        REQUIRE( !strcmp(cString4->getString(), "mValue3") );

        FilesystemUtils::remove_if(filesystem, [] (const char*) {return true;});
#else
        mocpp_deinitialize();

        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
#endif

        //config accessibility / permissions
        vs = getOcppContext()->getModel().getVariableService();
        Variable::Mutability mutability = Variable::Mutability::ReadWrite;
        Variable::AttributeTypeSet attrs = Variable::AttributeType::Actual;
        bool rebootRequired = false;
        bool isAccessible = true;
        auto cInt6 = vs->declareVariable<int>("mComponent", "cInt", 42, MO_VARIABLE_VOLATILE, mutability, attrs, rebootRequired, isAccessible);
        REQUIRE( cInt6->getMutability() == Variable::Mutability::ReadWrite );
        REQUIRE( !cInt6->isRebootRequired() );
        REQUIRE( vs->declareVariable<int>("mComponent", "cInt", 42) );

        //revoke permissions
        mutability = Variable::Mutability::ReadOnly;
        rebootRequired = true;
        vs->declareVariable<int>("mComponent", "cInt", 42, MO_VARIABLE_VOLATILE, mutability, attrs, rebootRequired, isAccessible);
        REQUIRE( cInt6->getMutability() == mutability );
        REQUIRE( cInt6->isRebootRequired() );

        //revoked permissions cannot be reverted
        mutability = Variable::Mutability::ReadWrite;
        rebootRequired = false;
        auto cInt7 = vs->declareVariable<int>("mComponent", "cInt", 42, MO_VARIABLE_VOLATILE, mutability, attrs, rebootRequired, isAccessible);
        REQUIRE( cInt7->getMutability() == Variable::Mutability::ReadOnly );
        REQUIRE( cInt7->isRebootRequired() );

        //accessibility cannot be changed after first initialization
        isAccessible = false;
        vs->declareVariable<int>("mComponent", "cInt", 42, MO_VARIABLE_VOLATILE, mutability, attrs, rebootRequired, isAccessible);
        vs->declareVariable<int>("mComponent", "cInt2", 42, MO_VARIABLE_VOLATILE, mutability, attrs, rebootRequired, isAccessible);
        Variable *result = nullptr;
        REQUIRE( vs->getVariable(Variable::AttributeType::Actual, "mComponent", "cInt", &result) == GetVariableStatus::Accepted );
        REQUIRE( result != nullptr );
        result = nullptr;
        REQUIRE( vs->getVariable(Variable::AttributeType::Actual, "mComponent", "cInt2", &result) == GetVariableStatus::Accepted );
        REQUIRE( result != nullptr );

        //create config in hidden container
        isAccessible = false;
        auto cHidden = vs->declareVariable<int>("mComponent", "cHidden", 42, MO_VARIABLE_VOLATILE "/hidden.json", mutability, attrs, rebootRequired, isAccessible);
        (void)cHidden;
        result = nullptr;
        REQUIRE( vs->getVariable(Variable::AttributeType::Actual, "mComponent", "cHidden", &result) == GetVariableStatus::Accepted );
        REQUIRE( result != nullptr );
    }

#if 0
    SECTION("Main lib integration") {

        //basic lifecycle
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        REQUIRE( getVariablePublic("ConnectionTimeOut") );
        REQUIRE( !getVariableContainersPublic().empty() );
        mocpp_deinitialize();
        REQUIRE( !getVariablePublic("ConnectionTimeOut") );
        REQUIRE( getVariableContainersPublic().empty() );

        //modify standard config ConnectionTimeOut. This config is not modified by the main lib during normal initialization / deinitialization
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        auto config = getVariablePublic("ConnectionTimeOut");

        config->setInt(1234); //update
        configuration_save(); //write back

        mocpp_deinitialize();

        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        REQUIRE( getVariablePublic("ConnectionTimeOut")->getInt() == 1234 );

        mocpp_deinitialize();
    }
#endif

#if 0
    SECTION("GetVariables") {

        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        loop();

        vs->declareVariable<int>(KNOWN_KEY, 1234, MO_FILENAME_PREFIX "persistent1.jsn", false);

        bool checkProcessed = false;

        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "GetVariables",
                [] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
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

        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "GetVariable",
                [] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1) + JSON_ARRAY_SIZE(2)));
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

        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        loop();

        vs->declareVariable<int>(KNOWN_KEY, 0, MO_FILENAME_PREFIX "persistent1.jsn", false);

        //update existing config
        bool checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "ChangeVariable",
                [] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(2)));
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
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "ChangeVariable",
                [] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(2)));
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
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "ChangeVariable",
                [] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(2)));
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
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "ChangeVariable",
                [] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(2)));
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
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "ChangeVariable",
                [] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(2)));
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

        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));

        auto connectionTimeout2 = vs->declareVariable<int>("ConnectionTimeOut", 4321);
        REQUIRE( connectionTimeout2->getInt() == 1234 );
        REQUIRE( connectionTimeout2 == factoryConnectionTimeOut );

        configuration_save();
        mocpp_deinitialize();

        //this time, factory default is not given (will lead to duplicates, should be considered in sanitization)
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        REQUIRE( getVariablePublic("ConnectionTimeOut")->getInt() != 1234 );
        mocpp_deinitialize();

        //provide factory default again
        configuration_init(filesystem);
        vs->declareVariable<int>("ConnectionTimeOut", 4321, MO_FILENAME_PREFIX "factory.jsn");
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        REQUIRE( getVariablePublic("ConnectionTimeOut")->getInt() == 1234 );
        mocpp_deinitialize();

    }
#endif

    mocpp_deinitialize();
}

#endif // MO_ENABLE_V201
