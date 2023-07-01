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

    REQUIRE(!getOcppContext());
}

#include <ArduinoOcpp_c.h>
#include <ArduinoOcpp/Core/ConfigurationOptions.h>

std::array<bool, 1024> checkpointsc {false};
size_t ncheckc = 0;

TEST_CASE( "C API test" ) {

    //initialize Context with dummy socket
    struct AO_FilesystemOpt fsopt;
    fsopt.use = true;
    fsopt.mount = true;
    fsopt.formatFsOnFail = true;

    LoopbackConnection loopback;
    ao_initialize(reinterpret_cast<AO_Connection*>(&loopback), "test-runner1234", "vendor", fsopt);
    
    auto context = getOcppContext();
    auto& model = context->getModel();

    ao_set_timer(custom_timer_cb);

    model.getClock().setTime(BASE_TIME);

    ao_endTransaction(NULL, NULL);

    SECTION("Run all functions") {

        ao_setConnectorPluggedInput([] () -> bool {checkpointsc[0] = true; return true;}); ncheckc++;
        ao_setConnectorPluggedInput_m(2, [] (unsigned int) -> bool {checkpointsc[1] = true; return true;}); ncheckc++;
        ao_setEnergyMeterInput([] () -> int {checkpointsc[2] = true; return 0;}); ncheckc++;
        ao_setEnergyMeterInput_m(2, [] (unsigned int) -> int {checkpointsc[3] = true; return 0;}); ncheckc++;
        ao_setPowerMeterInput([] () -> float {checkpointsc[4] = true; return 0.f;}); ncheckc++;
        ao_setPowerMeterInput_m(2, [] (unsigned int) -> float {checkpointsc[5] = true; return 0.f;}); ncheckc++;
        ao_setSmartChargingPowerOutput([] (float) {}); //overridden by CurrentOutput
        ao_setSmartChargingPowerOutput_m(2, [] (unsigned int, float) {}); //overridden by CurrentOutput
        ao_setSmartChargingCurrentOutput([] (float) {}); //overridden by generic SmartChargingOutput
        ao_setSmartChargingCurrentOutput_m(2, [] (unsigned int, float) {}); //overridden by generic SmartChargingOutput
        ao_setSmartChargingOutput([] (float, float, int) {checkpointsc[6] = true;}); ncheckc++;
        ao_setSmartChargingOutput_m(2, [] (unsigned int, float, float, int) {checkpointsc[7] = true;}); ncheckc++;
        ao_setEvReadyInput([] () -> bool {checkpointsc[8] = true; return true;}); ncheckc++;
        ao_setEvReadyInput_m(2, [] (unsigned int) -> bool {checkpointsc[9] = true; return true;}); ncheckc++;
        ao_setEvseReadyInput([] () -> bool {checkpointsc[10] = true; return true;}); ncheckc++;
        ao_setEvseReadyInput_m(2, [] (unsigned int) -> bool {checkpointsc[11] = true; return true;}); ncheckc++;
        ao_addErrorCodeInput([] () -> const char* {checkpointsc[12] = true; return nullptr;}); ncheckc++;
        ao_addErrorCodeInput_m(2, [] (unsigned int) -> const char* {checkpointsc[13] = true; return nullptr;}); ncheckc++;
        ao_addMeterValueInputInt([] () -> int {checkpointsc[14] = true; return 0;}, "Current.Import", "A", NULL, NULL); ncheckc++;
        ao_addMeterValueInputInt_m(2, [] (unsigned int) -> int {checkpointsc[15] = true; return 0;}, "Current.Import", "A", NULL, NULL); ncheckc++;
        
        ArduinoOcpp::SampledValueProperties svprops;
        svprops.setMeasurand("Current.Offered");
        auto valueSampler = std::unique_ptr<ArduinoOcpp::SampledValueSamplerConcrete<int32_t, ArduinoOcpp::SampledValueDeSerializer<int32_t>>>(
                                        new ArduinoOcpp::SampledValueSamplerConcrete<int32_t, ArduinoOcpp::SampledValueDeSerializer<int32_t>>(
                    svprops,
                    [] (ArduinoOcpp::ReadingContext) -> int32_t {checkpointsc[16] = true; return 0;})); ncheckc++;
        ao_addMeterValueInput(reinterpret_cast<MeterValueInput*>(valueSampler.release()));

        valueSampler = std::unique_ptr<ArduinoOcpp::SampledValueSamplerConcrete<int32_t, ArduinoOcpp::SampledValueDeSerializer<int32_t>>>(
                                        new ArduinoOcpp::SampledValueSamplerConcrete<int32_t, ArduinoOcpp::SampledValueDeSerializer<int32_t>>(
                    svprops,
                    [] (ArduinoOcpp::ReadingContext) -> int32_t {checkpointsc[17] = true; return 0;})); ncheckc++;
        ao_addMeterValueInput_m(2, reinterpret_cast<MeterValueInput*>(valueSampler.release()));

        ao_setOccupiedInput([] () -> bool {checkpointsc[18] = true; return true;}); ncheckc++;
        ao_setOccupiedInput_m(2, [] (unsigned int) -> bool {checkpointsc[19] = true; return true;}); ncheckc++;
        ao_setStartTxReadyInput([] () -> bool {checkpointsc[20] = true; return true;}); ncheckc++;
        ao_setStartTxReadyInput_m(2, [] (unsigned int) -> bool {checkpointsc[21] = true; return true;}); ncheckc++;
        ao_setStopTxReadyInput([] () -> bool {checkpointsc[22] = true; return true;}); ncheckc++;
        ao_setStopTxReadyInput_m(2, [] (unsigned int) -> bool {checkpointsc[23] = true; return true;}); ncheckc++;
        ao_setTxNotificationOutput([] (AO_TxNotification, AO_Transaction*) {checkpointsc[24] = true;}); ncheckc++;
        ao_setTxNotificationOutput_m(2, [] (unsigned int, AO_TxNotification, AO_Transaction*) {checkpointsc[25] = true;}); ncheckc++;
        ao_setOnUnlockConnectorInOut([] () -> OptionalBool {checkpointsc[26] = true; return OptionalBool::OptionalTrue;}); ncheckc++;
        ao_setOnUnlockConnectorInOut_m(2, [] (unsigned int) -> OptionalBool {checkpointsc[27] = true; return OptionalBool::OptionalTrue;}); ncheckc++;

        ao_setOnResetNotify([] (bool) -> bool {checkpointsc[28] = true; return true;}); ncheckc++;
        ao_setOnResetExecute([] (bool) {checkpointsc[29] = true;}); ncheckc++;

        ao_setOnReceiveRequest("StatusNotification", [] (const char*,size_t) {checkpointsc[30] = true;}); ncheckc++;
        ao_setOnSendConf("StatusNotification", [] (const char*,size_t) {checkpointsc[31] = true;}); ncheckc++;

        //set configuration which uses all Inputs and Outputs

        auto MeterValuesSampledData = ArduinoOcpp::declareConfiguration<const char*>("MeterValuesSampledData","", CONFIGURATION_FN);
        *MeterValuesSampledData = "Energy.Active.Import.Register,Power.Active.Import,Current.Import,Current.Offered";

        std::string out = SCPROFILE;
        loopback.sendTXT(out);

        //run tx management

        ao_loop();

        loop();

        ao_beginTransaction("mIdTag");
        ao_beginTransaction_m(2, "mIdTag");

        loop();

        REQUIRE(ao_isTransactionActive());
        REQUIRE(ao_isTransactionActive_m(2));
        REQUIRE(ao_isTransactionRunning());
        REQUIRE(ao_isTransactionRunning_m(2));
        REQUIRE(ao_getTransactionIdTag() != nullptr);
        REQUIRE(ao_getTransactionIdTag_m(2) != nullptr);
        REQUIRE(ao_getTransaction() != nullptr);
        REQUIRE(ao_getTransaction_m(2) != nullptr);
        REQUIRE(ao_ocppPermitsCharge());
        REQUIRE(ao_ocppPermitsCharge_m(2));

        ao_endTransaction(NULL, NULL);
        ao_endTransaction_m(2, NULL, NULL);

        loop();

        ao_beginTransaction_authorized("mIdTag", NULL);
        ao_beginTransaction_authorized_m(2, "mIdTag", NULL);

        loop();

        mtime += 3600 * 1000;

        loop();

        ao_endTransaction(NULL, NULL);
        ao_endTransaction_m(2, NULL, NULL);

        loop();

        ao_authorize("mIdTag", [] (const char*,const char*,size_t,void*) {checkpointsc[32] = true;}, NULL, NULL, NULL, NULL); ncheckc++;
        ao_startTransaction("mIdTag", [] (const char*,size_t) {checkpointsc[33] = true;}, NULL, NULL, NULL); ncheckc++;

        loop();

        ao_stopTransaction([] (const char*,size_t) {checkpointsc[34] = true;}, NULL, NULL, NULL); ncheckc++;

        //occupied Input will be validated when vehiclePlugged is false or undefined
        ao_setConnectorPluggedInput([] () {return false;});
        ao_setConnectorPluggedInput_m(2,[] (unsigned int) {return false;});

        loop();

        //run device management

        REQUIRE(ao_isOperative());
        REQUIRE(ao_isOperative_m(2));

        sendCustomRequest("UnlockConnector", [] () {
            auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
            (*doc)["connectorId"] = 1;
            return doc;
        }, [] (JsonObject) {});
        sendCustomRequest("UnlockConnector", [] () {
            auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
            (*doc)["connectorId"] = 2;
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
        
        AO_DBG_DEBUG("added %zu checkpoints", ncheckc);

        bool checkpointsPassed = true;
        for (unsigned int i = 0; i < ncheckc; i++) {
            if (!checkpointsc[i]) {
                AO_DBG_ERR("missed checkpoint %u", i);
                checkpointsPassed = false;
            }
        }

        REQUIRE(checkpointsPassed);
    }

    ao_deinitialize();

    REQUIRE(!getOcppContext());
}
