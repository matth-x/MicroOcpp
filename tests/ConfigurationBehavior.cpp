// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Debug.h>
#include <MicroOcpp/Version.h>
#include <catch2/catch.hpp>
#include "./helpers/testHelper.h"

using namespace MicroOcpp;

class CustomAuthorize : public Operation {
private:
    const char *status;
public:
    CustomAuthorize(const char *status) : status(status) { };
    const char *getOperationType() override {return "Authorize";}
    void processReq(JsonObject payload) override {
        //ignore payload - result is determined at construction time
    }
    std::unique_ptr<MemJsonDoc> createConf() override {
        auto res = makeMemJsonDoc("UnitTests", 2 * JSON_OBJECT_SIZE(1));
        auto payload = res->to<JsonObject>();
        payload["idTagInfo"]["status"] = status;
        return res;
    }
};

class CustomStartTransaction : public Operation {
private:
    const char *status;
public:
    CustomStartTransaction(const char *status) : status(status) { };
    const char *getOperationType() override {return "StartTransaction";}
    void processReq(JsonObject payload) override {
        //ignore payload - result is determined at construction time
    }
    std::unique_ptr<MemJsonDoc> createConf() override {
        auto res = makeMemJsonDoc("UnitTests", JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(1));
        auto payload = res->to<JsonObject>();
        payload["idTagInfo"]["status"] = status;
        payload["transactionId"] = 1000;
        return res;
    }
};

TEST_CASE( "Configuration Behavior" ) {
    printf("\nRun %s\n",  "Configuration Behavior");

    //clean state
    auto filesystem = makeDefaultFilesystemAdapter(FilesystemOpt::Use_Mount_FormatOnFail);
    MO_DBG_DEBUG("remove all");
    FilesystemUtils::remove_if(filesystem, [] (const char*) {return true;});

    //initialize Context with dummy socket
    LoopbackConnection loopback;
    mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));

    auto engine = getOcppContext();
    auto& checkMsg = engine->getOperationRegistry();

    auto connector = engine->getModel().getConnector(1);

    mocpp_set_timer(custom_timer_cb);

    loop();

    SECTION("StopTransactionOnEVSideDisconnect") {
        setConnectorPluggedInput([] () {return true;});
        startTransaction("mIdTag");
        loop();

        auto configBool = declareConfiguration<bool>("StopTransactionOnEVSideDisconnect", true);

        SECTION("set true") {
            configBool->setBool(true);

            setConnectorPluggedInput([] () {return false;});
            loop();

            REQUIRE(connector->getStatus() == ChargePointStatus_Available);
        }

        SECTION("set false") {
            configBool->setBool(false);

            setConnectorPluggedInput([] () {return false;});
            loop();

            REQUIRE(connector->getStatus() == ChargePointStatus_SuspendedEV);

            endTransaction();
            loop();

            REQUIRE(connector->getStatus() == ChargePointStatus_Available);
        }
    }

    SECTION("StopTransactionOnInvalidId") {
        auto configBool = declareConfiguration<bool>("StopTransactionOnInvalidId", true);

        checkMsg.registerOperation("Authorize", [] () {return new CustomAuthorize("Invalid");});
        checkMsg.registerOperation("StartTransaction", [] () {return new CustomStartTransaction("Invalid");});

        SECTION("set true") {
            configBool->setBool(true);

            beginTransaction("mIdTag_invalid");
            loop();

            REQUIRE(connector->getStatus() == ChargePointStatus_Available);

            beginTransaction_authorized("mIdTag_invalid2");
            loop();

            REQUIRE(connector->getStatus() == ChargePointStatus_Available);
        }

        SECTION("set false") {
            configBool->setBool(false);

            beginTransaction("mIdTag");
            loop();

            REQUIRE(connector->getStatus() == ChargePointStatus_Available);

            beginTransaction_authorized("mIdTag");
            loop();

            REQUIRE(connector->getStatus() == ChargePointStatus_SuspendedEVSE);

            endTransaction();
            loop();

            REQUIRE(connector->getStatus() == ChargePointStatus_Available);
        }
    }

    SECTION("AllowOfflineTxForUnknownId") {
        auto configBool = declareConfiguration<bool>("AllowOfflineTxForUnknownId", true);
        auto authorizationTimeoutInt = declareConfiguration<int>(MO_CONFIG_EXT_PREFIX "AuthorizationTimeout", 1);
        authorizationTimeoutInt->setInt(1); //try normal Authorize for 1s, then enter offline mode

        loopback.setOnline(false); //connection loss

        SECTION("set true") {
            configBool->setBool(true);

            beginTransaction("mIdTag");
            loop();

            REQUIRE(connector->getStatus() == ChargePointStatus_Charging);

            endTransaction();
            loop();

            REQUIRE(connector->getStatus() == ChargePointStatus_Available);
        }

        SECTION("set false") {
            configBool->setBool(false);

            beginTransaction("mIdTag");
            REQUIRE(connector->getStatus() == ChargePointStatus_Preparing);

            loop();

            REQUIRE(connector->getStatus() == ChargePointStatus_Available);
        }

        endTransaction();
        loopback.setOnline(true);
    }

#if MO_ENABLE_LOCAL_AUTH
    SECTION("LocalPreAuthorize") {
        auto configBool = declareConfiguration<bool>("LocalPreAuthorize", true);
        auto authorizationTimeoutInt = declareConfiguration<int>(MO_CONFIG_EXT_PREFIX "AuthorizationTimeout", 20);
        authorizationTimeoutInt->setInt(300); //try normal Authorize for 5 minutes

        declareConfiguration<bool>("LocalAuthorizeOffline", true)->setBool(true);
        declareConfiguration<bool>("LocalAuthListEnabled", true)->setBool(true);

        //define local auth list with entry local-idtag
        const char *localListMsg = "[2,\"testmsg-01\",\"SendLocalList\",{\"listVersion\":1,\"localAuthorizationList\":[{\"idTag\":\"local-idtag\",\"idTagInfo\":{\"status\":\"Accepted\"}}],\"updateType\":\"Full\"}]";
        loopback.sendTXT(localListMsg, strlen(localListMsg));
        loop();

        loopback.setOnline(false); //connection loss

        SECTION("set true - accepted idtag") {
            configBool->setBool(true);

            beginTransaction("local-idtag");
            loop();

            REQUIRE(connector->getStatus() == ChargePointStatus_Charging);
        }

        SECTION("set false") {
            configBool->setBool(false);

            beginTransaction("local-idtag");
            loop();

            REQUIRE(connector->getStatus() == ChargePointStatus_Preparing);

            loopback.setOnline(true);
            mtime += 20000; //Authorize will be retried after a few seconds
            loop();

            REQUIRE(connector->getStatus() == ChargePointStatus_Charging);
        }

        endTransaction();
        loopback.setOnline(true);
    }
#endif //MO_ENABLE_LOCAL_AUTH

    mocpp_deinitialize();
}
