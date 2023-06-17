#include <ArduinoOcpp.h>
#include <ArduinoOcpp/Core/Connection.h>
#include <ArduinoOcpp/Context.h>
#include <ArduinoOcpp/Model/Model.h>
#include <ArduinoOcpp/Core/Operation.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Core/FilesystemUtils.h>
#include "./catch2/catch.hpp"
#include "./helpers/testHelper.h"

using namespace ArduinoOcpp;

class CustomAuthorize : public Operation {
private:
    const char *status;
public:
    CustomAuthorize(const char *status) : status(status) { };
    const char *getOperationType() override {return "Authorize";}
    void processReq(JsonObject payload) override {
        //ignore payload - result is determined at construction time
    }
    std::unique_ptr<DynamicJsonDocument> createConf() override {
        auto res = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(2 * JSON_OBJECT_SIZE(1)));
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
    std::unique_ptr<DynamicJsonDocument> createConf() override {
        auto res = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(1)));
        auto payload = res->to<JsonObject>();
        payload["idTagInfo"]["status"] = status;
        payload["transactionId"] = 1000;
        return res;
    }
};

TEST_CASE( "Configuration Behavior" ) {

    //clean state
    auto filesystem = makeDefaultFilesystemAdapter(ArduinoOcpp::FilesystemOpt::Use_Mount_FormatOnFail);
    AO_DBG_DEBUG("remove all");
    FilesystemUtils::remove_all(filesystem, "");

    //initialize Context with dummy socket
    LoopbackConnection loopback;
    OCPP_initialize(loopback, ChargerCredentials("test-runner1234"));

    auto engine = getOcppContext();
    auto& checkMsg = engine->getOperationRegistry();

    auto connector = engine->getModel().getConnector(1);

    ao_set_timer(custom_timer_cb);

    loop();

    SECTION("StopTransactionOnEVSideDisconnect") {
        setConnectorPluggedInput([] () {return true;});
        startTransaction("mIdTag");
        loop();

        auto config = declareConfiguration<bool>("StopTransactionOnEVSideDisconnect", true);

        SECTION("set true") {
            *config = true;

            setConnectorPluggedInput([] () {return false;});
            loop();

            REQUIRE(connector->inferenceStatus() == OcppEvseState::Available);
        }

        SECTION("set false") {
            *config = false;

            setConnectorPluggedInput([] () {return false;});
            loop();

            REQUIRE(connector->inferenceStatus() == OcppEvseState::SuspendedEV);

            endTransaction();
            loop();

            REQUIRE(connector->inferenceStatus() == OcppEvseState::Available);
        }
    }

    SECTION("StopTransactionOnInvalidId") {
        auto config = declareConfiguration<bool>("StopTransactionOnInvalidId", true);

        checkMsg.registerOperation("Authorize", [] () {return new CustomAuthorize("Invalid");});
        checkMsg.registerOperation("StartTransaction", [] () {return new CustomStartTransaction("Invalid");});

        SECTION("set true") {
            *config = true;

            beginTransaction("mIdTag");
            loop();

            REQUIRE(connector->inferenceStatus() == OcppEvseState::Available);

            beginTransaction_authorized("mIdTag");
            loop();

            REQUIRE(connector->inferenceStatus() == OcppEvseState::Available);
        }

        SECTION("set false") {
            *config = false;

            beginTransaction("mIdTag");
            loop();

            REQUIRE(connector->inferenceStatus() == OcppEvseState::Available);

            beginTransaction_authorized("mIdTag");
            loop();

            REQUIRE(connector->inferenceStatus() == OcppEvseState::SuspendedEVSE);

            endTransaction();
            loop();

            REQUIRE(connector->inferenceStatus() == OcppEvseState::Available);
        }
    }

    SECTION("AllowOfflineTxForUnknownId") {
        auto config = declareConfiguration<bool>("AllowOfflineTxForUnknownId", true);
        auto authorizationTimeout = ArduinoOcpp::declareConfiguration<int>("AO_AuthorizationTimeout", 1);
        *authorizationTimeout = 1; //try normal Authorize for 1s, then enter offline mode

        loopback.setConnected(false); //connection loss

        SECTION("set true") {
            *config = true;

            beginTransaction("mIdTag");
            loop();

            REQUIRE(connector->inferenceStatus() == OcppEvseState::Charging);

            endTransaction();
            loop();

            REQUIRE(connector->inferenceStatus() == OcppEvseState::Available);
        }

        SECTION("set false") {
            *config = false;

            beginTransaction("mIdTag");
            REQUIRE(connector->inferenceStatus() == OcppEvseState::Preparing);

            loop();

            REQUIRE(connector->inferenceStatus() == OcppEvseState::Available);
        }

        endTransaction();
        loopback.setConnected(true);
    }

    SECTION("LocalPreAuthorize") {
        auto config = declareConfiguration<bool>("LocalPreAuthorize", true);
        auto authorizationTimeout = ArduinoOcpp::declareConfiguration<int>("AO_AuthorizationTimeout", 20);
        *authorizationTimeout = 300; //try normal Authorize for 5 minutes

        auto localAuthorizeOffline = declareConfiguration<bool>("LocalAuthorizeOffline", true, CONFIGURATION_FN, true, true, true, false);
        *localAuthorizeOffline = true;
        auto localAuthListEnabled = declareConfiguration<bool>("LocalAuthListEnabled", true, CONFIGURATION_FN, true, true, true, false);
        *localAuthListEnabled = true;

        //define local auth list with entry local-idtag
        std::string localListMsg = "[2,\"testmsg-01\",\"SendLocalList\",{\"listVersion\":1,\"localAuthorizationList\":[{\"idTag\":\"local-idtag\",\"idTagInfo\":{\"status\":\"Accepted\"}}],\"updateType\":\"Full\"}]";
        loopback.sendTXT(localListMsg);
        loop();

        loopback.setConnected(false); //connection loss

        SECTION("set true - accepted idtag") {
            *config = true;

            beginTransaction("local-idtag");
            loop();

            REQUIRE(connector->inferenceStatus() == OcppEvseState::Charging);
        }

        SECTION("set false") {
            *config = false;

            beginTransaction("local-idtag");
            loop();

            REQUIRE(connector->inferenceStatus() == OcppEvseState::Preparing);

            loopback.setConnected(true);
            mtime += 20000; //Authorize will be retried after a few seconds
            loop();

            REQUIRE(connector->inferenceStatus() == OcppEvseState::Charging);
        }

        endTransaction();
        loopback.setConnected(true);
    }

    OCPP_deinitialize();
}
