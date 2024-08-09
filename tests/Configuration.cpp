// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <string.h>

#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include "./catch2/catch.hpp"
#include "./helpers/testHelper.h"

#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/ConfigurationContainer.h>
#include <MicroOcpp/Core/ConfigurationContainerFlash.h>
#include <MicroOcpp/Core/Configuration_c.h>

#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Operations/CustomOperation.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

#define GET_CONFIG_ALL "[2,\"test-msg\",\"GetConfiguration\",{}]"
#define KNOWN_KEY "__ExistingKey"
#define UNKOWN_KEY "__UnknownKey"
#define GET_CONFIG_KNOWN_UNKOWN "[2,\"test-mst\",\"GetConfiguration\",{\"key\":[\"" KNOWN_KEY "\",\"" UNKOWN_KEY "\"]}]"

// some globals for the C-API tests
bool g_checkProcessed [10];
ocpp_configuration g_configs [2];
int g_config_values [2];
uint16_t g_config_write_count[2];

TEST_CASE( "Configuration" ) {
    printf("\nRun %s\n",  "Configuration");

    //clean state
    auto filesystem = makeDefaultFilesystemAdapter(FilesystemOpt::Use_Mount_FormatOnFail);
    FilesystemUtils::remove_if(filesystem, [] (const char*) {return true;});

    LoopbackConnection loopback; //initialize Context with dummy socket

    mocpp_set_timer(custom_timer_cb);

    SECTION("Basic container operations"){
        std::unique_ptr<ConfigurationContainer> container;

        SECTION("Volatile storage") {
            container = makeConfigurationContainerVolatile(CONFIGURATION_VOLATILE "/volatile1", true);
        }

        SECTION("Persistent storage") {
            container = makeConfigurationContainerFlash(filesystem, MO_FILENAME_PREFIX "persistent1.jsn", true);
        }

        //check emptyness
        REQUIRE( container->size() == 0 );

        //add first config, fetch by index
        auto configFirst = container->createConfiguration(TConfig::Int, "cFirst");
        REQUIRE( container->size() == 1 );
        REQUIRE( container->getConfiguration((size_t) 0) == configFirst.get());

        //add one config of each type
        auto cInt = container->createConfiguration(TConfig::Int, "cInt");
        auto cBool = container->createConfiguration(TConfig::Bool, "cBool");
        auto cString = container->createConfiguration(TConfig::String, "cString");
        
        REQUIRE( container->size() == 4 );

        //fetch config by key
        REQUIRE( container->getConfiguration(cBool->getKey()) == cBool);

        //remove config
        container->remove(cBool.get());
        REQUIRE( container->size() == 3 );
        REQUIRE( container->getConfiguration(cBool->getKey()) == nullptr);

        //clean container
        container->remove(container->getConfiguration((size_t) 0));
        container->remove(container->getConfiguration((size_t) 0));
        container->remove(container->getConfiguration((size_t) 0));
        REQUIRE( container->size() == 0 );
    }

    SECTION("Persistency on filesystem") {

        auto container = makeConfigurationContainerFlash(filesystem, MO_FILENAME_PREFIX "persistent1.jsn", true);

        //trivial load call
        REQUIRE( container->load() );
        REQUIRE( container->size() == 0 );

        //add config, store, load again
        auto cString = container->createConfiguration(TConfig::String, "cString");
        cString->setString("mValue");

        REQUIRE( container->save() ); //store

        container.reset(); //destroy

        //...load again
        auto container2 = makeConfigurationContainerFlash(filesystem, MO_FILENAME_PREFIX "persistent1.jsn", true);
        REQUIRE( container2->size() == 0 );
        REQUIRE( container2->load() );
        REQUIRE( container2->size() == 1 );

        auto cString2 = container2->getConfiguration("cString");
        REQUIRE( cString2 != nullptr );
        REQUIRE( !strcmp(cString2->getString(), "mValue") );
    }

    SECTION("Configuration API") {

        //declare configs
        REQUIRE( configuration_init(filesystem) );
        auto cInt = declareConfiguration<int>("cInt", 42);
        REQUIRE( cInt != nullptr );
        declareConfiguration<bool>("cBool", true);
        declareConfiguration<const char*>("cString", "mValue");

        //fetch config
        REQUIRE( getConfigurationPublic("cInt")->getInt() == 42 );

        //store, destroy, reload
        REQUIRE( configuration_save() );
        cInt.reset();
        configuration_deinit();
        REQUIRE( getConfigurationPublic("cInt") == nullptr);

        REQUIRE( configuration_init(filesystem) ); //reload

        //fetch configs (declare with different factory default - should remain at original value)
        auto cInt2 = declareConfiguration<int>("cInt", -1);
        auto cBool2 = declareConfiguration<bool>("cBool", false);
        auto cString2 = declareConfiguration<const char*>("cString", "no effect");
        REQUIRE( configuration_load() ); //load config objects with stored values

        //check load result
        REQUIRE( cInt2->getInt() == 42 );
        REQUIRE( cBool2->getBool() == true );
        REQUIRE( !strcmp(cString2->getString(), "mValue") );

        //declare config twice
        auto cInt3 = declareConfiguration<int>("cInt", -1);
        REQUIRE( cInt3 == cInt2 );

        //declare config twice but in different container
        auto cInt4 = declareConfiguration<int>("cInt", -1, CONFIGURATION_VOLATILE);
        REQUIRE( cInt4 == cInt2 );

        //declare config twice but with conflicting type (will supersede old type because to simplify FW upgrades)
        auto cNewType = declareConfiguration<const char*>("cInt", "mValue2");
        REQUIRE( cNewType != cInt2 );
        REQUIRE( !strcmp(cNewType->getString(), "mValue2") );
        
        //store, destroy, reload
        REQUIRE( configuration_save() );
        configuration_deinit();
        REQUIRE( getConfigurationPublic("cInt") == nullptr);
        REQUIRE( configuration_init(filesystem) ); //reload
        auto cNewType2 = declareConfiguration<const char*>("cInt", "no effect");
        REQUIRE( configuration_load() );
        REQUIRE( !strcmp(cNewType2->getString(), "mValue2") );

        //get config before declared (container needs to be declared already at this point)
        auto cString3 = getConfigurationPublic("cString");
        REQUIRE( !strcmp(cString3->getString(), "mValue") );
        configuration_deinit();

        //value needs to outlive container
        configuration_init(filesystem);
        auto cString4 = declareConfiguration<const char *>("cString2", "mValue3");
        configuration_deinit();
        REQUIRE( !strcmp(cString4->getString(), "mValue3") );

        FilesystemUtils::remove_if(filesystem, [] (const char*) {return true;});

        //config accessibility / permissions
        configuration_init(filesystem);
        bool readonly = false;
        bool rebootRequired = false;
        bool isPublic = true;
        auto cInt6 = declareConfiguration<int>("cInt", 42, CONFIGURATION_FN, readonly, rebootRequired, isPublic);
        REQUIRE( !cInt6->isReadOnly() );
        REQUIRE( !cInt6->isRebootRequired() );
        REQUIRE( getConfigurationPublic("cInt") );

        //revoke permissions
        readonly = true;
        rebootRequired = true;
        declareConfiguration<int>("cInt", 42, CONFIGURATION_FN, readonly, rebootRequired, isPublic);
        REQUIRE( cInt6->isReadOnly() );
        REQUIRE( cInt6->isRebootRequired() );

        //revoked permissions cannot be reverted
        readonly = false;
        rebootRequired = false;
        auto cInt7 = declareConfiguration<int>("cInt", 42, CONFIGURATION_FN, readonly, rebootRequired, isPublic);
        REQUIRE( cInt7->isReadOnly() );
        REQUIRE( cInt7->isRebootRequired() );

        //accessibility cannot be changed after first initialization
        isPublic = false;
        declareConfiguration<int>("cInt", 42, CONFIGURATION_FN, false, rebootRequired, isPublic);
        declareConfiguration<int>("cInt2", 42, CONFIGURATION_FN, false, rebootRequired, isPublic);
        REQUIRE( getConfigurationPublic("cInt") );
        REQUIRE( getConfigurationPublic("cInt2") );

        //create config in hidden container
        isPublic = false;
        auto cHidden = declareConfiguration<int>("cHidden", 42, MO_FILENAME_PREFIX "hidden.jsn", false, false, isPublic);
        REQUIRE( !getConfigurationPublic("cHidden") );
        
        //hidden container cannot be fetched
        auto configsPublic = getConfigurationContainersPublic();
        REQUIRE( configsPublic.size() == 1 );

        configuration_deinit();
    }

    SECTION("ContainerFlash memory optimization") {

        //key storage optimization: the static key provided by declareConfiguration is preferred. If
        //no static key is available for the config (if the config is loaded from flash without being
        //declared before), then a key on the heap is allocated. If the config is later allocated,
        //the heap-key is replaced by the static key

        const char *key_static = "cInt";

        configuration_init(filesystem);
        auto cInt = declareConfiguration<int>(key_static, 42);
        configuration_save();
        configuration_deinit();

        configuration_init(filesystem);
        declareConfiguration<int>("dummy", 23); //dummy config to declare CONFIGURATION_FN
        configuration_load();
        const char *key_heap = getConfigurationPublic(key_static)->getKey();
        REQUIRE( key_heap != key_static );
        
        declareConfiguration<int>(key_static, 42); //replace heap key with static key here

        const char *key_replaced = getConfigurationPublic(key_static)->getKey();
        REQUIRE( key_replaced == key_static );

        configuration_deinit();
    }

    SECTION("Main lib integration") {

        //basic lifecycle
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        REQUIRE( getConfigurationPublic("ConnectionTimeOut") );
        REQUIRE( !getConfigurationContainersPublic().empty() );
        mocpp_deinitialize();
        REQUIRE( !getConfigurationPublic("ConnectionTimeOut") );
        REQUIRE( getConfigurationContainersPublic().empty() );

        //modify standard config ConnectionTimeOut. This config is not modified by the main lib during normal initialization / deinitialization
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        auto config = getConfigurationPublic("ConnectionTimeOut");

        config->setInt(1234); //update
        configuration_save(); //write back

        mocpp_deinitialize();

        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        REQUIRE( getConfigurationPublic("ConnectionTimeOut")->getInt() == 1234 );

        mocpp_deinitialize();
    }

    SECTION("GetConfiguration") {

        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        loop();

        declareConfiguration<int>(KNOWN_KEY, 1234, MO_FILENAME_PREFIX "persistent1.jsn", false);

        bool checkProcessed = false;

        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "GetConfiguration",
                [] () {
                    //create req
                    auto doc = std::unique_ptr<MemJsonDoc>(new MemJsonDoc(JSON_OBJECT_SIZE(1)));
                    doc->to<JsonObject>();
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
                "GetConfiguration",
                [] () {
                    //create req
                    auto doc = std::unique_ptr<MemJsonDoc>(new MemJsonDoc(JSON_OBJECT_SIZE(1) + JSON_ARRAY_SIZE(2)));
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

    SECTION("ChangeConfiguration") {

        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        loop();

        declareConfiguration<int>(KNOWN_KEY, 0, MO_FILENAME_PREFIX "persistent1.jsn", false);

        //update existing config
        bool checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "ChangeConfiguration",
                [] () {
                    //create req
                    auto doc = std::unique_ptr<MemJsonDoc>(new MemJsonDoc(JSON_OBJECT_SIZE(2)));
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
        REQUIRE( getConfigurationPublic(KNOWN_KEY)->getInt() == 1234 );

        //try to update not existing key
        checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "ChangeConfiguration",
                [] () {
                    //create req
                    auto doc = std::unique_ptr<MemJsonDoc>(new MemJsonDoc(JSON_OBJECT_SIZE(2)));
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
                "ChangeConfiguration",
                [] () {
                    //create req
                    auto doc = std::unique_ptr<MemJsonDoc>(new MemJsonDoc(JSON_OBJECT_SIZE(2)));
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
        registerConfigurationValidator(KNOWN_KEY, [] (const char *v) {
            return v[0] == '1';
        });

        //validation success
        checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "ChangeConfiguration",
                [] () {
                    //create req
                    auto doc = std::unique_ptr<MemJsonDoc>(new MemJsonDoc(JSON_OBJECT_SIZE(2)));
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
        REQUIRE( getConfigurationPublic(KNOWN_KEY)->getInt() == 100234 );

        //validation failure
        checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "ChangeConfiguration",
                [] () {
                    //create req
                    auto doc = std::unique_ptr<MemJsonDoc>(new MemJsonDoc(JSON_OBJECT_SIZE(2)));
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
        REQUIRE( getConfigurationPublic(KNOWN_KEY)->getInt() == 100234 ); //keep old value

        mocpp_deinitialize();
    }

    SECTION("Define factory defaults for standard configs") {

        //set factory default for standard config ConnectionTimeOut
        configuration_init(filesystem);
        auto factoryConnectionTimeOut = declareConfiguration<int>("ConnectionTimeOut", 1234, MO_FILENAME_PREFIX "factory.jsn");

        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));

        auto connectionTimeout2 = declareConfiguration<int>("ConnectionTimeOut", 4321);
        REQUIRE( connectionTimeout2->getInt() == 1234 );
        REQUIRE( connectionTimeout2 == factoryConnectionTimeOut );

        configuration_save();
        mocpp_deinitialize();

        //this time, factory default is not given (will lead to duplicates, should be considered in sanitization)
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        REQUIRE( getConfigurationPublic("ConnectionTimeOut")->getInt() != 1234 );
        mocpp_deinitialize();

        //provide factory default again
        configuration_init(filesystem);
        declareConfiguration<int>("ConnectionTimeOut", 4321, MO_FILENAME_PREFIX "factory.jsn");
        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        REQUIRE( getConfigurationPublic("ConnectionTimeOut")->getInt() == 1234 );
        mocpp_deinitialize();

    }

    SECTION("C-API") {
        ocpp_configuration_container container;
        memset(&container, 0, sizeof(container));

        bool check_load = false;

        container.load = [] (void *user_data) {
            g_checkProcessed[0] = true;
            return true;
        };

        container.save = [] (void *user_data) {
            g_checkProcessed[1] = true;
            return true;
        };

        ocpp_configuration *config_predefined = &g_configs[0];
        config_predefined->get_key = [] (void *user_data) -> const char* {return "ConnectionTimeOut";}; // existing OCPP key to use custom config store
        config_predefined->get_type = [] (void *user_data) -> ocpp_config_datatype {return ENUM_CDT_INT;};
        config_predefined->set_int = [] (void *user_data, int val) -> void {g_config_values[0] = val;};
        config_predefined->get_int = [] (void *user_data) -> int {return g_config_values[0];};
        config_predefined->get_write_count = [] (void *user_data) -> uint16_t {return g_config_write_count[0];};

        container.create_configuration = [] (void *user_data, ocpp_config_datatype dt, const char *key) -> ocpp_configuration* {
            ocpp_configuration *config_created = &g_configs[1];
            config_created->get_key = [] (void *user_data) -> const char* {return "MCreatedConfig";}; // non-existing key to test create_configuration function
            config_created->get_type = [] (void *user_data) -> ocpp_config_datatype {return ENUM_CDT_INT;};
            config_created->set_int = [] (void *user_data, int val) -> void {g_config_values[1] = val;};
            config_created->get_int = [] (void *user_data) -> int {return g_config_values[1];};
            config_created->get_write_count = [] (void *user_data) -> uint16_t {return g_config_write_count[1];};
            return config_created;
        };

        container.remove = [] (void *user_data, const char *key) -> void {
            g_checkProcessed[2] = true;
        };

        container.size = [] (void *user_data) {
            return sizeof(g_configs) / sizeof(g_configs[0]);
        };

        container.get_configuration = [] (void *user_data, size_t i) -> ocpp_configuration* {
            return &g_configs[i];
        };

        container.get_configuration_by_key = [] (void *user_data, const char *key) -> ocpp_configuration* {
            if (!strcmp(key, "ConnectionTimeOut")) {
                return &g_configs[0];
            } else if (!strcmp(key, "MCreatedConfig")) {
                return g_configs[1].get_key ? 
                    &g_configs[1] : // createConfig has already been called
                    nullptr; // config hasn't been created yet
            }
            return nullptr;
        };

        ocpp_configuration_container_add(&container, "MContainerPath", true);

        mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));
        loop();

        REQUIRE( g_checkProcessed[0] );

        auto test_predefined = declareConfiguration<int>("ConnectionTimeOut", 0);

        test_predefined->setInt(12345);
        REQUIRE( test_predefined->getInt() == 12345 );
        REQUIRE( config_predefined->get_int(config_predefined->user_data) == 12345 );

        g_checkProcessed[1] = false; // check if store is executed
        test_predefined->setInt(555);
        g_config_write_count[0];

        configuration_save();

        REQUIRE( g_checkProcessed[1] );

        // test if declaring new configs is handled
        auto test_created = declareConfiguration<int>("MCreatedConfig", 123, "MContainerPath");
        REQUIRE( test_created != nullptr );
        REQUIRE( test_created->getInt() == 123 );
        ocpp_configuration *config_created = &g_configs[1];
        REQUIRE( config_created->get_int(config_created->user_data) == 123 );

        mocpp_deinitialize();
    }

}
