#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>
#include "./catch2/catch.hpp"
#include "./helpers/testHelper.h"

#define BASE_TIME "2023-01-01T00:00:00.000Z"
#define SCPROFILE "[2,\"testmsg\",\"SetChargingProfile\",{\"connectorId\":0,\"csChargingProfiles\":{\"chargingProfileId\":0,\"stackLevel\":0,\"chargingProfilePurpose\":\"ChargePointMaxProfile\",\"chargingProfileKind\":\"Absolute\",\"chargingSchedule\":{\"duration\":1000000,\"startSchedule\":\"2023-01-01T00:00:00.000Z\",\"chargingRateUnit\":\"W\",\"chargingSchedulePeriod\":[{\"startPeriod\":0,\"limit\":16,\"numberPhases\":3}]}}}]"

TEST_CASE( "C++ API test" ) {
    printf("\nRun %s\n",  "C++ API test");

    //initialize Context with dummy socket
    MicroOcpp::LoopbackConnection loopback;
    mocpp_initialize(loopback, ChargerCredentials("test-runner1234"));

    auto context = getOcppContext();
    auto& model = context->getModel();

    mocpp_set_timer(custom_timer_cb);

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
        addErrorDataInput([c = &checkpoints[ncheck++]] () -> MicroOcpp::ErrorData {*c = true; return nullptr;});
        addMeterValueInput([c = &checkpoints[ncheck++]] () -> float {*c = true; return 0.f;}, "Current.Import");

        MicroOcpp::SampledValueProperties svprops;
        svprops.setMeasurand("Current.Offered");
        auto valueSampler = std::unique_ptr<MicroOcpp::SampledValueSamplerConcrete<int32_t, MicroOcpp::SampledValueDeSerializer<int32_t>>>(
                                        new MicroOcpp::SampledValueSamplerConcrete<int32_t, MicroOcpp::SampledValueDeSerializer<int32_t>>(
                    svprops,
                    [c = &checkpoints[ncheck++]] (MicroOcpp::ReadingContext) -> int32_t {*c = true; return 0;}));
        addMeterValueInput(std::move(valueSampler));

        setOccupiedInput([c = &checkpoints[ncheck++]] () -> bool {*c = true; return false;});
        setStartTxReadyInput([c = &checkpoints[ncheck++]] () -> bool {*c = true; return true;});
        setStopTxReadyInput([c = &checkpoints[ncheck++]] () -> bool {*c = true; return true;});
        setTxNotificationOutput([c = &checkpoints[ncheck++]] (MicroOcpp::Transaction*, MicroOcpp::TxNotification) {*c = true;});
        setOnUnlockConnectorInOut([c = &checkpoints[ncheck++]] () -> MicroOcpp::PollResult<bool> {*c = true; return true;});

        setOnResetNotify([c = &checkpoints[ncheck++]] (bool) -> bool {*c = true; return true;});
        setOnResetExecute([c = &checkpoints[ncheck++]] (bool) {*c = true;});

        REQUIRE( getFirmwareService() != nullptr );
        REQUIRE( getDiagnosticsService() != nullptr );
        REQUIRE( getOcppContext() != nullptr );

        setOnReceiveRequest("StatusNotification", [c = &checkpoints[ncheck++]] (JsonObject) {*c = true;});
        setOnSendConf("StatusNotification", [c = &checkpoints[ncheck++]] (JsonObject) {*c = true;});
        sendRequest("DataTransfer", [c = &checkpoints[ncheck++]] () {
            *c = true;
            auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
            doc->to<JsonObject>();
            return doc;
        }, [c = &checkpoints[ncheck++]] (JsonObject) {*c = true;});
        setRequestHandler("DataTransfer", [c = &checkpoints[ncheck++]] (JsonObject) {*c = true;}, [c = &checkpoints[ncheck++]] () {
            *c = true;
            auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
            doc->to<JsonObject>();
            return doc;
        });

        //set configuration which uses all Inputs and Outputs

        auto MeterValuesSampledDataString = MicroOcpp::declareConfiguration<const char*>("MeterValuesSampledData","", CONFIGURATION_FN);
        MeterValuesSampledDataString->setString("Energy.Active.Import.Register,Power.Active.Import,Current.Import,Current.Offered");

        loopback.sendTXT(SCPROFILE, strlen(SCPROFILE));

        //run tx management

        mocpp_loop();

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

        sendRequest("UnlockConnector", [] () {
            auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
            (*doc)["connectorId"] = 1;
            return doc;
        }, [] (JsonObject) {});

        sendRequest("Reset", [] () {
            auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
            (*doc)["type"] = "Hard";
            return doc;
        }, [] (JsonObject) {});

        loop();

        mtime += 3600 * 1000;

        loop();
        
        MO_DBG_DEBUG("added %zu checkpoints", ncheck);

        bool checkpointsPassed = true;
        for (unsigned int i = 0; i < ncheck; i++) {
            if (!checkpoints[i]) {
                MO_DBG_ERR("missed checkpoint %u", i);
                checkpointsPassed = false;
            }
        }

        REQUIRE(checkpointsPassed);
    }

    mocpp_deinitialize();

    REQUIRE(!getOcppContext());
}

#include <MicroOcpp_c.h>
#include <MicroOcpp/Core/ConfigurationOptions.h>

std::array<bool, 1024> checkpointsc {false};
size_t ncheckc = 0;

TEST_CASE( "C API test" ) {

    //initialize Context with dummy socket
    struct OCPP_FilesystemOpt fsopt;
    fsopt.use = true;
    fsopt.mount = true;
    fsopt.formatFsOnFail = true;

    MicroOcpp::LoopbackConnection loopback;
    ocpp_initialize(reinterpret_cast<OCPP_Connection*>(&loopback), "test-runner1234", "vendor", fsopt, false);
    
    auto context = getOcppContext();
    auto& model = context->getModel();

    mocpp_set_timer(custom_timer_cb);

    model.getClock().setTime(BASE_TIME);

    ocpp_endTransaction(NULL, NULL);

    SECTION("Run all functions") {

        ocpp_setConnectorPluggedInput([] () -> bool {checkpointsc[0] = true; return true;}); ncheckc++;
        ocpp_setConnectorPluggedInput_m(2, [] (unsigned int) -> bool {checkpointsc[1] = true; return true;}); ncheckc++;
        ocpp_setEnergyMeterInput([] () -> int {checkpointsc[2] = true; return 0;}); ncheckc++;
        ocpp_setEnergyMeterInput_m(2, [] (unsigned int) -> int {checkpointsc[3] = true; return 0;}); ncheckc++;
        ocpp_setPowerMeterInput([] () -> float {checkpointsc[4] = true; return 0.f;}); ncheckc++;
        ocpp_setPowerMeterInput_m(2, [] (unsigned int) -> float {checkpointsc[5] = true; return 0.f;}); ncheckc++;
        ocpp_setSmartChargingPowerOutput([] (float) {}); //overridden by CurrentOutput
        ocpp_setSmartChargingPowerOutput_m(2, [] (unsigned int, float) {}); //overridden by CurrentOutput
        ocpp_setSmartChargingCurrentOutput([] (float) {}); //overridden by generic SmartChargingOutput
        ocpp_setSmartChargingCurrentOutput_m(2, [] (unsigned int, float) {}); //overridden by generic SmartChargingOutput
        ocpp_setSmartChargingOutput([] (float, float, int) {checkpointsc[6] = true;}); ncheckc++;
        ocpp_setSmartChargingOutput_m(2, [] (unsigned int, float, float, int) {checkpointsc[7] = true;}); ncheckc++;
        ocpp_setEvReadyInput([] () -> bool {checkpointsc[8] = true; return true;}); ncheckc++;
        ocpp_setEvReadyInput_m(2, [] (unsigned int) -> bool {checkpointsc[9] = true; return true;}); ncheckc++;
        ocpp_setEvseReadyInput([] () -> bool {checkpointsc[10] = true; return true;}); ncheckc++;
        ocpp_setEvseReadyInput_m(2, [] (unsigned int) -> bool {checkpointsc[11] = true; return true;}); ncheckc++;
        ocpp_addErrorCodeInput([] () -> const char* {checkpointsc[12] = true; return nullptr;}); ncheckc++;
        ocpp_addErrorCodeInput_m(2, [] (unsigned int) -> const char* {checkpointsc[13] = true; return nullptr;}); ncheckc++;
        ocpp_addMeterValueInputFloat([] () -> float {checkpointsc[14] = true; return 0.f;}, "Current.Import", "A", NULL, NULL); ncheckc++;
        ocpp_addMeterValueInputFloat_m(2, [] (unsigned int) -> float {checkpointsc[15] = true; return 0.f;}, "Current.Import", "A", NULL, NULL); ncheckc++;
        
        MicroOcpp::SampledValueProperties svprops;
        svprops.setMeasurand("Current.Offered");
        auto valueSampler = std::unique_ptr<MicroOcpp::SampledValueSamplerConcrete<int32_t, MicroOcpp::SampledValueDeSerializer<int32_t>>>(
                                        new MicroOcpp::SampledValueSamplerConcrete<int32_t, MicroOcpp::SampledValueDeSerializer<int32_t>>(
                    svprops,
                    [] (MicroOcpp::ReadingContext) -> int32_t {checkpointsc[16] = true; return 0;})); ncheckc++;
        ocpp_addMeterValueInput(reinterpret_cast<MeterValueInput*>(valueSampler.release()));

        valueSampler = std::unique_ptr<MicroOcpp::SampledValueSamplerConcrete<int32_t, MicroOcpp::SampledValueDeSerializer<int32_t>>>(
                                        new MicroOcpp::SampledValueSamplerConcrete<int32_t, MicroOcpp::SampledValueDeSerializer<int32_t>>(
                    svprops,
                    [] (MicroOcpp::ReadingContext) -> int32_t {checkpointsc[17] = true; return 0;})); ncheckc++;
        ocpp_addMeterValueInput_m(2, reinterpret_cast<MeterValueInput*>(valueSampler.release()));

        ocpp_setOccupiedInput([] () -> bool {checkpointsc[18] = true; return true;}); ncheckc++;
        ocpp_setOccupiedInput_m(2, [] (unsigned int) -> bool {checkpointsc[19] = true; return true;}); ncheckc++;
        ocpp_setStartTxReadyInput([] () -> bool {checkpointsc[20] = true; return true;}); ncheckc++;
        ocpp_setStartTxReadyInput_m(2, [] (unsigned int) -> bool {checkpointsc[21] = true; return true;}); ncheckc++;
        ocpp_setStopTxReadyInput([] () -> bool {checkpointsc[22] = true; return true;}); ncheckc++;
        ocpp_setStopTxReadyInput_m(2, [] (unsigned int) -> bool {checkpointsc[23] = true; return true;}); ncheckc++;
        ocpp_setTxNotificationOutput([] (OCPP_Transaction*, OCPP_TxNotification) {checkpointsc[24] = true;}); ncheckc++;
        ocpp_setTxNotificationOutput_m(2, [] (unsigned int, OCPP_Transaction*, OCPP_TxNotification) {checkpointsc[25] = true;}); ncheckc++;
        ocpp_setOnUnlockConnectorInOut([] () -> OptionalBool {checkpointsc[26] = true; return OptionalBool::OptionalTrue;}); ncheckc++;
        ocpp_setOnUnlockConnectorInOut_m(2, [] (unsigned int) -> OptionalBool {checkpointsc[27] = true; return OptionalBool::OptionalTrue;}); ncheckc++;

        ocpp_setOnResetNotify([] (bool) -> bool {checkpointsc[28] = true; return true;}); ncheckc++;
        ocpp_setOnResetExecute([] (bool) {checkpointsc[29] = true;}); ncheckc++;

        ocpp_setOnReceiveRequest("StatusNotification", [] (const char*,size_t) {checkpointsc[30] = true;}); ncheckc++;
        ocpp_setOnSendConf("StatusNotification", [] (const char*,size_t) {checkpointsc[31] = true;}); ncheckc++;

        //set configuration which uses all Inputs and Outputs

        auto MeterValuesSampledDataString = MicroOcpp::declareConfiguration<const char*>("MeterValuesSampledData","", CONFIGURATION_FN);
        MeterValuesSampledDataString->setString("Energy.Active.Import.Register,Power.Active.Import,Current.Import,Current.Offered");

        loopback.sendTXT(SCPROFILE, strlen(SCPROFILE));

        //run tx management

        ocpp_loop();

        loop();

        ocpp_beginTransaction("mIdTag");
        ocpp_beginTransaction_m(2, "mIdTag");

        loop();

        REQUIRE(ocpp_isTransactionActive());
        REQUIRE(ocpp_isTransactionActive_m(2));
        REQUIRE(ocpp_isTransactionRunning());
        REQUIRE(ocpp_isTransactionRunning_m(2));
        REQUIRE(ocpp_getTransactionIdTag() != nullptr);
        REQUIRE(ocpp_getTransactionIdTag_m(2) != nullptr);
        REQUIRE(ocpp_getTransaction() != nullptr);
        REQUIRE(ocpp_getTransaction_m(2) != nullptr);
        REQUIRE(ocpp_ocppPermitsCharge());
        REQUIRE(ocpp_ocppPermitsCharge_m(2));

        ocpp_endTransaction("mIdTag", NULL);
        ocpp_endTransaction_m(2, "mIdTag", NULL);

        loop();

        ocpp_beginTransaction_authorized("mIdTag", NULL);
        ocpp_beginTransaction_authorized_m(2, "mIdTag", NULL);

        loop();

        mtime += 3600 * 1000;

        loop();

        ocpp_endTransaction_authorized(NULL, NULL);
        ocpp_endTransaction_authorized_m(2, NULL, NULL);

        loop();

        ocpp_authorize("mIdTag", [] (const char*,const char*,size_t,void*) {checkpointsc[32] = true;}, NULL, NULL, NULL, NULL); ncheckc++;
        ocpp_startTransaction("mIdTag", [] (const char*,size_t) {checkpointsc[33] = true;}, NULL, NULL, NULL); ncheckc++;

        loop();

        ocpp_stopTransaction([] (const char*,size_t) {checkpointsc[34] = true;}, NULL, NULL, NULL); ncheckc++;

        //occupied Input will be validated when vehiclePlugged is false or undefined
        ocpp_setConnectorPluggedInput([] () {return false;});
        ocpp_setConnectorPluggedInput_m(2,[] (unsigned int) {return false;});

        loop();

        //run device management

        REQUIRE(ocpp_isOperative());
        REQUIRE(ocpp_isOperative_m(2));

        sendRequest("UnlockConnector", [] () {
            auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
            (*doc)["connectorId"] = 1;
            return doc;
        }, [] (JsonObject) {});
        sendRequest("UnlockConnector", [] () {
            auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
            (*doc)["connectorId"] = 2;
            return doc;
        }, [] (JsonObject) {});

        sendRequest("Reset", [] () {
            auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
            (*doc)["type"] = "Hard";
            return doc;
        }, [] (JsonObject) {});

        loop();

        mtime += 3600 * 1000;

        loop();
        
        MO_DBG_DEBUG("added %zu checkpoints", ncheckc);

        bool checkpointsPassed = true;
        for (unsigned int i = 0; i < ncheckc; i++) {
            if (!checkpointsc[i]) {
                MO_DBG_ERR("missed checkpoint %u", i);
                checkpointsPassed = false;
            }
        }

        REQUIRE(checkpointsPassed);
    }

    ocpp_deinitialize();

    REQUIRE(!getOcppContext());
}
