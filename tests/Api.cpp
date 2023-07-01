#include <ArduinoOcpp.h>
#include <ArduinoOcpp/Core/Connection.h>
#include <ArduinoOcpp/Context.h>
#include <ArduinoOcpp/Model/Model.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Core/SimpleRequestFactory.h>
#include <ArduinoOcpp/Model/SmartCharging/SmartChargingService.h>
#include <ArduinoOcpp/Operations/CustomOperation.h>
#include "./catch2/catch.hpp"
#include "./helpers/testHelper.h"

#define BASE_TIME "2023-01-01T00:00:00.000Z"
#define SCPROFILE "[2,\"testmsg\",\"SetChargingProfile\",{\"connectorId\":0,\"csChargingProfiles\":{\"chargingProfileId\":0,\"stackLevel\":0,\"chargingProfilePurpose\":\"ChargePointMaxProfile\",\"chargingProfileKind\":\"Absolute\",\"chargingSchedule\":{\"duration\":1000000,\"startSchedule\":\"2023-01-01T00:00:00.000Z\",\"chargingRateUnit\":\"W\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":16,\"numberPhases\":3}]}}}]";

using namespace ArduinoOcpp;

TEST_CASE( "C++ API test" ) {

    //initialize Context with dummy socket
    LoopbackConnection loopback;
    OCPP_initialize(loopback, ChargerCredentials("test-runner1234"));

    auto context = getOcppContext();
    auto& model = context->getModel();

    ao_set_timer(custom_timer_cb);

    model.getClock().setTime(BASE_TIME);

    endTransaction();

    SECTION("Run all functions") {

        //Set all possible Inputs and outputs
        std::array<bool, 1024> checkpoints {false};
        size_t ncheck = 0;

        setConnectorPluggedInput([c = &checkpoints[ncheck++]] () -> bool {*c = true; return true;});
        setEnergyMeterInput([c = &checkpoints[ncheck++]] () -> float {*c = true; return 0.f;});
        setPowerMeterInput([c = &checkpoints[ncheck++]] () -> float {*c = true; return 0.f;});
        setSmartChargingPowerOutput([] (float) {}); //overridden by CurrentOutput
        setSmartChargingCurrentOutput([] (float) {}); //overridden by generic SmartChargingOutput
        setSmartChargingOutput([c = &checkpoints[ncheck++]] (float, float, int) {*c = true;});
        setEvReadyInput([c = &checkpoints[ncheck++]] () -> bool {*c = true; return true;});
        setEvseReadyInput([c = &checkpoints[ncheck++]] () -> bool {*c = true; return true;});
        addErrorCodeInput([c = &checkpoints[ncheck++]] () -> const char* {*c = true; return nullptr;});
        addErrorDataInput([c = &checkpoints[ncheck++]] () -> ArduinoOcpp::ErrorData {*c = true; return nullptr;});
        addMeterValueInput([c = &checkpoints[ncheck++]] () -> int32_t {*c = true; return 0.f;},
                "Current.Import");

        ArduinoOcpp::SampledValueProperties svprops;
        svprops.setMeasurand("Current.Offered");
        auto valueSampler = std::unique_ptr<ArduinoOcpp::SampledValueSamplerConcrete<int32_t, ArduinoOcpp::SampledValueDeSerializer<int32_t>>>(
                                        new ArduinoOcpp::SampledValueSamplerConcrete<int32_t, ArduinoOcpp::SampledValueDeSerializer<int32_t>>(
                    svprops,
                    [c = &checkpoints[ncheck++]] (ArduinoOcpp::ReadingContext) -> int32_t {*c = true; return 0;}));
        addMeterValueInput(std::move(valueSampler));

        setOccupiedInput([c = &checkpoints[ncheck++]] () -> bool {*c = true; return false;});
        setStartTxReadyInput([c = &checkpoints[ncheck++]] () -> bool {*c = true; return true;});
        setStopTxReadyInput([c = &checkpoints[ncheck++]] () -> bool {*c = true; return true;});
        setTxNotificationOutput([c = &checkpoints[ncheck++]] (ArduinoOcpp::TxNotification,ArduinoOcpp::Transaction*) {*c = true;});
        setOnUnlockConnectorInOut([c = &checkpoints[ncheck++]] () -> ArduinoOcpp::PollResult<bool> {*c = true; return true;});

        setOnResetNotify([c = &checkpoints[ncheck++]] (bool) -> bool {*c = true; return true;});
        setOnResetExecute([c = &checkpoints[ncheck++]] (bool) {*c = true;});

        REQUIRE( getFirmwareService() != nullptr );
        REQUIRE( getDiagnosticsService() != nullptr );
        REQUIRE( getOcppContext() != nullptr );

        setOnReceiveRequest("StatusNotification", [c = &checkpoints[ncheck++]] (JsonObject) {*c = true;});
        setOnSendConf("StatusNotification", [c = &checkpoints[ncheck++]] (JsonObject) {*c = true;});
        sendCustomRequest("DataTransfer", [c = &checkpoints[ncheck++]] () {
            *c = true;
            auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
            doc->to<JsonObject>();
            return doc;
        }, [c = &checkpoints[ncheck++]] (JsonObject) {*c = true;});
        setCustomRequestHandler("DataTransfer", [c = &checkpoints[ncheck++]] (JsonObject) {*c = true;}, [c = &checkpoints[ncheck++]] () {
            *c = true;
            auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
            doc->to<JsonObject>();
            return doc;
        });

        //set configuration which uses all Inputs and Outputs

        auto MeterValuesSampledData = ArduinoOcpp::declareConfiguration<const char*>("MeterValuesSampledData","", CONFIGURATION_FN);
        *MeterValuesSampledData = "Energy.Active.Import.Register,Power.Active.Import,Current.Import,Current.Offered";

        std::string out = SCPROFILE;
        loopback.sendTXT(out);

        //run tx management

        OCPP_loop();

        loop();

        beginTransaction("mIdTag");

        loop();

        REQUIRE(isTransactionActive());
        REQUIRE(isTransactionRunning());
        REQUIRE(getTransactionIdTag() != nullptr);
        REQUIRE(getTransaction() != nullptr);
        REQUIRE(ocppPermitsCharge());

        endTransaction();

        loop();

        beginTransaction_authorized("mIdTag");

        loop();

        mtime += 3600 * 1000;

        loop();

        endTransaction();

        loop();

        authorize("mIdTag", [c = &checkpoints[ncheck++]] (JsonObject) {*c = true;});
        startTransaction("mIdTag", [c = &checkpoints[ncheck++]] (JsonObject) {*c = true;});

        loop();

        stopTransaction([c = &checkpoints[ncheck++]] (JsonObject) {*c = true;});

        //occupied Input will be validated when vehiclePlugged is false or undefined
        setConnectorPluggedInput(nullptr);

        loop();

        //run device management

        REQUIRE(isOperative());

        sendCustomRequest("UnlockConnector", [] () {
            auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
            (*doc)["connectorId"] = 1;
            return doc;
        }, [] (JsonObject) {});

        sendCustomRequest("Reset", [] () {
            auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
            (*doc)["type"] = "Hard";
            return doc;
        }, [] (JsonObject) {});

        loop();

        mtime += 3600 * 1000;

        loop();
        
        AO_DBG_DEBUG("added %zu checkpoints", ncheck);

        bool checkpointsPassed = true;
        for (unsigned int i = 0; i < ncheck; i++) {
            if (!checkpoints[i]) {
                AO_DBG_ERR("missed checkpoint %u", i);
                checkpointsPassed = false;
            }
        }

        REQUIRE(checkpointsPassed);
    }

    OCPP_deinitialize();
}
