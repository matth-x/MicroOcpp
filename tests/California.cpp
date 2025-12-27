// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp.h>
#include <MicroOcpp/Version.h>
#include <MicroOcpp/Core/Connection.h>
#include <catch2/catch.hpp>
#include "./helpers/testHelper.h"

#include <MicroOcpp/Model/California/CaliforniaUtils.h>
#include <MicroOcpp/Model/California/CaliforniaService.h>
#include <MicroOcpp/Operations/DataTransferSetUserPrice.h>
#include <MicroOcpp/Operations/DataTransferFinalCost.h>
#include <MicroOcpp/Operations/DataTransferRunningCost.h>
#include <MicroOcpp/Debug.h>


#define CUSTOM_DISPLAY_COSTANDPRICE         "[2, \"testmsg\", \"ChangeConfiguration\", {\"key\": \"CustomDisplayCostAndPrice\", \"value\": \"true\"}]"
#define MULT_LANGUAGE_COSTANDPRICE          "[2, \"testmsg\", \"ChangeConfiguration\", {\"key\": \"CustomMultiLanguageMessages\", \"value\": \"true\"}]"

#define DEFAULT_PRICE_CONFIUARATION         "[2,\"testmsg\",\"ChangeConfiguration\",{\"key\":\"DefaultPrice\",\"value\":\"{\\\"priceText\\\": \\\"0.15 $/kWh, idle fee after charging: 1 $/hr\\\",\\\"priceTextOffline\\\": \\\"The station is offline. Charging is possible for 0.15 $/kWh.\\\",\\\"chargingPrice\\\": { \\\"kWhPrice\\\": 0.15, \\\"hourPrice\\\": 0.00, \\\"flatFee\\\": 0.00 }}\"}]"

#define RUNING_COST_CHARGING_MSG            "[2,\"testmsg\",\"DataTransfer\",{\"vendorId\":\"org.openchargealliance.costmsg\",\"messageId\":\"RunningCost\",\"data\":\"{\\\"transactionId\\\": %d,\\\"timestamp\\\": \\\"2021-03-19T12:00:00Z\\\",\\\"meterValue\\\": 1234000,\\\"cost\\\": 12.34,\\\"state\\\": \\\"Charging\\\",\\\"chargingPrice\\\": {\\\"kWhPrice\\\": 0.20,\\\"hourPrice\\\": 1.00,\\\"flatFee\\\": 0.05},\\\"idlePrice\\\": {\\\"graceMinutes\\\": 30,\\\"hourPrice\\\": 1.50},\\\"nextPeriod\\\": {\\\"atTime\\\": \\\"2021-03-19T13:00:00Z\\\",\\\"chargingPrice\\\": {\\\"kWhPrice\\\": 0.25,\\\"hourPrice\\\": 1.20,\\\"flatFee\\\": 0.10},\\\"idlePrice\\\": {\\\"graceMinutes\\\": 15,\\\"hourPrice\\\": 2.00}},\\\"triggerMeterValue\\\": {\\\"atTime\\\": \\\"2021-03-19T12:30:00Z\\\",\\\"atEnergykWh\\\": 50,\\\"atPowerkW\\\": 0.1,\\\"atCPStatus\\\": [\\\"SuspendedEV\\\",\\\"SuspendedEVSE\\\"]}}\"}]"
#define RUNING_COST_IDLE_MSG                "[2,\"testmsg\",\"DataTransfer\",{\"vendorId\":\"org.openchargealliance.costmsg\",\"messageId\":\"RunningCost\",\"data\":\"{\\\"transactionId\\\": %d,\\\"timestamp\\\": \\\"2021-03-19T12:00:00Z\\\",\\\"meterValue\\\": 1234000,\\\"cost\\\": 12.34,\\\"state\\\": \\\"Idle\\\",    \\\"chargingPrice\\\": {\\\"kWhPrice\\\": 0.20,\\\"hourPrice\\\": 1.00,\\\"flatFee\\\": 0.05},\\\"idlePrice\\\": {\\\"graceMinutes\\\": 30,\\\"hourPrice\\\": 1.50},\\\"nextPeriod\\\": {\\\"atTime\\\": \\\"2021-03-19T13:00:00Z\\\",\\\"chargingPrice\\\": {\\\"kWhPrice\\\": 0.25,\\\"hourPrice\\\": 1.20,\\\"flatFee\\\": 0.10},\\\"idlePrice\\\": {\\\"graceMinutes\\\": 15,\\\"hourPrice\\\": 2.00}},\\\"triggerMeterValue\\\": {\\\"atTime\\\": \\\"2021-03-19T12:30:00Z\\\",\\\"atEnergykWh\\\": 50,\\\"atPowerkW\\\": 0.1,\\\"atCPStatus\\\": [\\\"SuspendedEV\\\",\\\"SuspendedEVSE\\\"]}}\"}]"

#define SETUSER_PRICE_MSG                   "[2,\"testmsg\",\"DataTransfer\",{\"vendorId\":\"org.openchargealliance.costmsg\",\"messageId\":\"SetUserPrice\",\"data\":\"{\\\"idToken\\\": \\\"ABC123\\\",\\\"priceText\\\": \\\"$0.12/kWh, no idle fee\\\"}\"}]"
#define SETUSER_PRICE_MULTI_LANGUAGE_MSG    "[2,\"testmsg\",\"DataTransfer\",{\"vendorId\":\"org.openchargealliance.costmsg\",\"messageId\":\"SetUserPrice\",\"data\":\"{\\\"idToken\\\": \\\"ABC123\\\",\\\"priceText\\\": \\\"$0.12/kWh, no idle fee\\\", \\\"priceTextExtra\\\": [{\\\"format\\\": \\\"UTF8\\\", \\\"language\\\": \\\"nl\\\", \\\"content\\\": \\\"€0.12/kWh, geen idle fee\\\"}, {\\\"format\\\": \\\"UTF8\\\", \\\"language\\\": \\\"de\\\", \\\"content\\\": \\\"€0,12/kWh, keine Leerlaufgebühr\\\"}}]}\" } ]"

#define FINAL_COST_MSG                      "[2,\"testmsg\",\"DataTransfer\",{\"vendorId\":\"org.openchargealliance.costmsg\",\"messageId\":\"FinalCost\",\"data\":\"{ \\\"transactionId\\\": %d, \\\"cost\\\": 3.31, \\\"priceText\\\": \\\"$2.81 @ $0.12/kWh, $0.50 @ $1/h, TOTAL KWH: 23.4 TIME: 03.50 COST: $3.31. Visit www.cpo.com/invoices/13546 for an invoice of your session.\\\", \\\"qrCodeText\\\": \\\"https://www.cpo.com/invoices/13546\\\" }\"}]"

TEST_CASE( "CaliforniaUtils" ) {
    printf("\nRun %s\n",  "CaliforniaUtils Tests");
    using namespace MicroOcpp::v16::CaliforniaUtils;

    auto ocppVersion = GENERATE(MO_OCPP_V16, MO_OCPP_V201);

    mo_initialize();
    mo_useMockServer();
    mo_setOcppVersion(ocppVersion);
    mo_setBootNotificationData("TestModel", "TestVendor");
    
    SECTION("DefaultPrice Validation") {
        // normal test
        REQUIRE( validateDefaultPrice("{\"priceText\": \"0.15 $/kWh, idle fee after charging: 1 $/hr\",\"priceTextOffline\": \"The station is offline. Charging is possible for 0.15 $/kWh.\",\"chargingPrice\": { \"kWhPrice\": 0.15, \"hourPrice\": 0.00, \"flatFee\": 0.00 }}", nullptr) );
        // without flatFee
        REQUIRE( validateDefaultPrice("{\"priceText\": \"0.15 $/kWh, idle fee after charging: 1 $/hr\",\"priceTextOffline\": \"The station is offline. Charging is possible for 0.15 $/kWh.\",\"chargingPrice\": { \"kWhPrice\": 0.15, \"hourPrice\": 0.00 }}", nullptr) );
        // without priceTextOffline, but existing chargingPrice
        REQUIRE_FALSE( validateDefaultPrice("{\"priceText\": \"0.15 $/kWh, idle fee after charging: 1 $/hr\",\"chargingPrice\": { \"kWhPrice\": 0.15, \"hourPrice\": 0.00 }}", nullptr) );
        // without chargingPrice, but existing priceTextOffline
        REQUIRE( validateDefaultPrice("{\"priceText\": \"0.15 $/kWh, idle fee after charging: 1 $/hr\",\"priceTextOffline\": \"The station is offline. Charging is possible for 0.15 $/kWh.\"}", nullptr) );
        // without priceTextOffline and priceText
        REQUIRE_FALSE( validateDefaultPrice("{\"chargingPrice\": { \"kWhPrice\": 0.15, \"hourPrice\": 0.00 }}", nullptr) );
        // without priceText
        REQUIRE_FALSE( validateDefaultPrice("{\"priceTextOffline\": \"The station is offline. Charging is possible for 0.15 $/kWh.\",\"chargingPrice\": { \"kWhPrice\": 0.15, \"hourPrice\": 0.00, \"flatFee\": 0.00 }}", nullptr) );

        // minimal valid
        REQUIRE( validateDefaultPrice("{\"priceText\": \"0.15 $/kWh, idle fee after charging: 1 $/hr\" }", nullptr) );
        // empty JSON without priceText
        REQUIRE_FALSE( validateDefaultPrice( "{  }",  nullptr) );
        // illegal content
        REQUIRE_FALSE( validateDefaultPrice("{ \"priceText\": \"0.15 $/kWh, idle fee after charging: 1 $/hr\", \"illegal\": \"empty content\" }", nullptr) );
        REQUIRE_FALSE( validateDefaultPrice( "",  nullptr) );
        REQUIRE_FALSE( validateDefaultPrice(nullptr, nullptr) );
    }

    SECTION("TimeOffset Validation") {
        // TimeOffset tests
        // Zero offset
        REQUIRE( validateTimeOffset("Z", nullptr) );
        // Normal offsets
        REQUIRE( validateTimeOffset("+02:00", nullptr) );
        REQUIRE( validateTimeOffset("-0230", nullptr) );
        REQUIRE( validateTimeOffset("+00:00", nullptr) );
        // Invalid offsets
        REQUIRE_FALSE( validateTimeOffset("2:00", nullptr) );
        // Missing minutes
        REQUIRE_FALSE( validateTimeOffset("", nullptr) );
        REQUIRE_FALSE( validateTimeOffset("", nullptr) );
        REQUIRE_FALSE( validateTimeOffset(nullptr, nullptr) );
    }

    SECTION("NextTimeOffset TransitionDateTime Validation") {
        // legal test
        REQUIRE( validateNextTimeOffsetTransitionDateTime("2023-10-29T03:00:00Z", mo_getApiContext()) );
        REQUIRE( validateNextTimeOffsetTransitionDateTime("2023-10-29T03:00:00+02:00", mo_getApiContext()) );
        // illegal tests
        REQUIRE_FALSE( validateNextTimeOffsetTransitionDateTime("2023-10-29 03:00:00", mo_getApiContext()) );
        REQUIRE_FALSE( validateNextTimeOffsetTransitionDateTime("InvalidDateTime", mo_getApiContext()) );
        REQUIRE_FALSE( validateNextTimeOffsetTransitionDateTime("", mo_getApiContext()) );
        REQUIRE_FALSE( validateNextTimeOffsetTransitionDateTime(nullptr, mo_getApiContext()) );
        REQUIRE_FALSE( validateNextTimeOffsetTransitionDateTime("2023-10-29T03:00:00Z", nullptr) );
    }

    SECTION("Supported Languages Validation") {
        // legal test
        REQUIRE( validateSupportLanguages("en,de,zh", nullptr) );
        REQUIRE( validateSupportLanguages("en-US,en-GB", nullptr) );
        // illegal tests
        REQUIRE_FALSE( validateSupportLanguages("x-xx1-11", nullptr) );
        REQUIRE_FALSE( validateSupportLanguages("00-123", nullptr) );
        REQUIRE_FALSE( validateSupportLanguages("123456", nullptr) );
        REQUIRE_FALSE( validateSupportLanguages("Invalid", nullptr) );
        REQUIRE_FALSE( validateSupportLanguages("", nullptr) );
        REQUIRE_FALSE( validateSupportLanguages(nullptr, nullptr) );
    }
}

TEST_CASE( "California's Operations" )
{
    printf("\nRun %s\n",  "California's Operations Tests");
    using namespace MicroOcpp::v16::CaliforniaUtils;

    auto ocppVersion = GENERATE(MO_OCPP_V16, MO_OCPP_V201);

    mo_initialize();
    mo_useMockServer();

    mo_setOcppVersion(ocppVersion);
    
    mo_setBootNotificationData("TestModel", "TestVendor");

    mo_getContext()->setTicksCb(custom_timer_cb);

    mo_setup();

    loop();

    MicroOcpp::v16::CaliforniaService *californiaService = mo_getContext()->getModel16().getCaliforniaService();
    REQUIRE( californiaService != nullptr );


    SECTION ("Configure CustomDisplayCostAndPrice to true") {
        mo_receiveTXT(mo_getApiContext(), CUSTOM_DISPLAY_COSTANDPRICE, strlen(CUSTOM_DISPLAY_COSTANDPRICE));
        loop();

        REQUIRE( californiaService->getCustomDisplayCostAndPrice() == true );
    }


    SECTION("User-specific Price") {
        // correct idToken
        mo_beginTransaction_authorized2(mo_getApiContext(), 1, "ABC123", nullptr);

        loop();

        auto californiaServiceEvse = californiaService->getEvse(1);
        californiaServiceEvse->setPriceTextOutput([](unsigned int evseId, const char* priceText, void* arg){
            REQUIRE(evseId == 1);
            REQUIRE(strcmp(priceText, "$0.12/kWh, no idle fee") == 0);
            REQUIRE(strcmp((const char*)arg, "TestArg") == 0);
        }, (void*)"TestArg");

        mo_receiveTXT(mo_getApiContext(), SETUSER_PRICE_MSG, strlen(SETUSER_PRICE_MSG));

        loop();


        californiaServiceEvse->setPriceTextOutput([](unsigned int evseId, const char* priceText, void* arg){
            REQUIRE(evseId == 1);
            REQUIRE(strcmp(priceText, "$0.12/kWh, no idle fee") == 0);
            REQUIRE(strcmp((const char*)arg, "TestArg") == 0);
        }, (void*)"TestArg");
        mo_endTransaction_authorized2(mo_getApiContext(), 1, "ABC123", nullptr);

        loop();

        // incorrect idToken
        mo_beginTransaction_authorized2(mo_getApiContext(), 1, "WRONGTOKEN", nullptr);
        loop();
        
        mo_receiveTXT(mo_getApiContext(), SETUSER_PRICE_MSG, strlen(SETUSER_PRICE_MSG));
        loop();

        mo_endTransaction_authorized2(mo_getApiContext(), 1, "WRONGTOKEN", nullptr);
        loop();
    }

    SECTION("Running Cost") {
        // correct idToken
        mo_beginTransaction_authorized2(mo_getApiContext(), 1, "ABC123", nullptr);

        loop();

        // Running 
        auto californiaServiceEvse = californiaService->getEvse(1);
        californiaServiceEvse->setkWhPriceOutput([](unsigned int evseId, float kWhPrice, void* arg){
            REQUIRE( evseId == 1 );
            REQUIRE( (kWhPrice > 0.19f && kWhPrice < 0.26f) );
            REQUIRE( strcmp((const char*)arg, "TestArg") == 0 );
        }, (void*)"TestArg");
        californiaServiceEvse->setHourPriceOutput([](unsigned int evseId, float hourPrice, void* arg){
            REQUIRE( evseId == 1 );
            REQUIRE( (hourPrice > 0.99f && hourPrice < 1.30f) );
            REQUIRE( strcmp((const char*)arg, "TestArg") == 0 );
        }, (void*)"TestArg");
        californiaServiceEvse->setFlatFeeOutput([](unsigned int evseId, float flatFee, void* arg){
            REQUIRE( evseId == 1 );
            REQUIRE( (flatFee > 0.04f && flatFee < 0.11f) );
            REQUIRE( strcmp((const char*)arg, "TestArg") == 0 );
        }, (void*)"TestArg");
        californiaServiceEvse->setCostOutput([](unsigned int evseId, float cost, void* arg){
            REQUIRE( evseId == 1 );
            REQUIRE( (cost > 12.33f) );
            REQUIRE( strcmp((const char*)arg, "TestArg") == 0 );
        }, (void*)"TestArg");
        californiaServiceEvse->setIdleGraceMinutesOutput([](unsigned int evseId, int idleGraceMinutes, void* arg){
            REQUIRE( evseId == 1 );
            REQUIRE( (idleGraceMinutes == 30 || idleGraceMinutes == 15) );
            REQUIRE( strcmp((const char*)arg, "TestArg") == 0 );
        }, (void*)"TestArg");
        californiaServiceEvse->setChargingStatusOutput([](unsigned int evseId, bool isCharging, void* arg){
            REQUIRE( evseId == 1 );
            REQUIRE( isCharging == true );
            REQUIRE( strcmp((const char*)arg, "TestArg") == 0 );
        }, (void*)"TestArg");

        char buffer[1024];
        auto txnr = mo_v16_getTransactionId2(mo_getApiContext(), 1);
        snprintf(buffer, sizeof(buffer), RUNING_COST_CHARGING_MSG, txnr);
        mo_receiveTXT(mo_getApiContext(), buffer, strlen(buffer));

        loop();

        loop();

        mo_endTransaction_authorized2(mo_getApiContext(), 1, "ABC123", nullptr);
        
        loop();
    }

    SECTION("Final Cost") {
        mo_beginTransaction_authorized2(mo_getApiContext(), 1, "ABC123", nullptr);

        loop();

        auto californiaServiceEvse = californiaService->getEvse(1);
        californiaServiceEvse->setPriceTextOutput([](unsigned int evseId, const char* priceText, void* arg){
            REQUIRE(evseId == 1);
            REQUIRE(strcmp(priceText, "$2.81 @ $0.12/kWh, $0.50 @ $1/h, TOTAL KWH: 23.4 TIME: 03.50 COST: $3.31. Visit www.cpo.com/invoices/13546 for an invoice of your session.") == 0);
            REQUIRE(strcmp((const char*)arg, "TestArg") == 0);
        }, (void*)"TestArg");
        californiaServiceEvse->setQrCodeTextOutput([](unsigned int evseId, const char* qrCodeText, void* arg){
            REQUIRE(evseId == 1);
            REQUIRE(strcmp(qrCodeText, "https://www.cpo.com/invoices/13546") == 0);
            REQUIRE(strcmp((const char*)arg, "TestArg") == 0);
        }, (void*)"TestArg");
        californiaServiceEvse->setCostOutput([](unsigned int evseId, float cost, void* arg){
            REQUIRE( evseId == 1 );
            REQUIRE( (cost > 3.30f && cost < 3.32f) );
            REQUIRE( strcmp((const char*)arg, "TestArg") == 0 );
        }, (void*)"TestArg");

        char buffer[1024];
        auto txnr = mo_v16_getTransactionId2(mo_getApiContext(), 1);
        snprintf(buffer, sizeof(buffer), FINAL_COST_MSG, txnr);
        mo_receiveTXT(mo_getApiContext(), buffer, strlen(buffer));

        loop();
        mo_endTransaction_authorized2(mo_getApiContext(), 1, "ABC123", nullptr);

        loop();
    }
}

TEST_CASE( "California's MultiLanguage" )
{
    
}

#if 0
TEST_CASE( "CaliforniaService" )
{
    printf("\nRun %s\n",  "CaliforniaService Tests");
    using namespace MicroOcpp;
    using namespace MicroOcpp::v16;

    mo_initialize();

    mo_setup();

    auto context = mo_getContext();
    REQUIRE( context != nullptr );
    mo_getContext()->setTicksCb(custom_timer_cb);

    auto& model16 = context->getModel16();

    auto californiaService = model16.getCaliforniaService();
    REQUIRE( californiaService != nullptr );

    // getEvse tests
    unsigned int numEvseId = model16.getNumEvseId();
    for (unsigned int i = 0; i < numEvseId; i++) {
        auto evse = californiaService->getEvse(i);
        REQUIRE( evse != nullptr );
        // REQUIRE( evse->setup() == true );
    }
    // out of bound evseId
    // REQUIRE( californiaService->getEvse(numEvseId) == nullptr );

    mo_deinitialize();
}

#endif