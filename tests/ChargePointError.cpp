// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include "./catch2/catch.hpp"
#include "./helpers/testHelper.h"

#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Operations/CustomOperation.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>

#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Core/Configuration.h>

#include <MicroOcpp/Model/ConnectorBase/Connector.h>
#include <MicroOcpp/Model/FirmwareManagement/FirmwareService.h>

#define BASE_TIME     "2023-01-01T00:00:00.000Z"
#define BASE_TIME_1H  "2023-01-01T01:00:00.000Z"
#define FTP_URL       "ftps://localhost/firmware.bin"

#define ERROR_INFO_EXAMPLE "error description"
#define ERROR_INFO_LOW_1   "low severity 1"
#define ERROR_INFO_LOW_2   "low severity 2"
#define ERROR_INFO_HIGH    "high severity"

#define ERROR_VENDOR_ID    "mVendorId"
#define ERROR_VENDOR_CODE  "mVendorErrorCode"

using namespace MicroOcpp;

TEST_CASE( "ChargePointError" ) {
    printf("\nRun %s\n",  "ChargePointError");

    //clean state
    auto filesystem = makeDefaultFilesystemAdapter(FilesystemOpt::Use_Mount_FormatOnFail);
    FilesystemUtils::remove_if(filesystem, [] (const char*) {return true;});

    //initialize Context with dummy socket
    LoopbackConnection loopback;

    mocpp_set_timer(custom_timer_cb);

    mocpp_initialize(loopback, ChargerCredentials("test-runner"));
    auto& model = getOcppContext()->getModel();
    auto fwService = getFirmwareService();
    SECTION("FirmwareService initialized") {
        REQUIRE(fwService != nullptr);
    }

    model.getClock().setTime(BASE_TIME);

    loop();

    SECTION("Err and resolve (soft error)") {

        bool errorCondition = false;

        addErrorDataInput([&errorCondition] () -> ErrorData {
            if (errorCondition) {
                ErrorData error = "OtherError";
                error.isFaulted = false;
                error.info = ERROR_INFO_EXAMPLE;
                error.vendorId = ERROR_VENDOR_ID;
                error.vendorErrorCode = ERROR_VENDOR_CODE;
                return error;
            }
            return nullptr;
        });

        //test error condition during transaction to check if status remains unchanged

        beginTransaction("mIdTag");

        loop();

        REQUIRE( getChargePointStatus() == ChargePointStatus_Charging );
        REQUIRE( isOperative() );

        bool checkProcessed = false;

        getOcppContext()->getOperationRegistry().registerOperation("StatusNotification",
            [&checkProcessed] () {
                return new Ocpp16::CustomOperation("StatusNotification",
                    [ &checkProcessed] (JsonObject payload) {
                        //process req
                        checkProcessed = true;
                        REQUIRE( !strcmp(payload["errorCode"] | "_Undefined", "OtherError") );
                        REQUIRE( !strcmp(payload["info"] | "_Undefined", ERROR_INFO_EXAMPLE) );
                        REQUIRE( !strcmp(payload["status"] | "_Undefined", "Charging") );
                        REQUIRE( !strcmp(payload["vendorId"] | "_Undefined", ERROR_VENDOR_ID) );
                        REQUIRE( !strcmp(payload["vendorErrorCode"] | "_Undefined", ERROR_VENDOR_CODE) );
                    },
                    [] () {
                        //create conf
                        return createEmptyDocument();
                    });
            });

        errorCondition = true;

        loop();

        REQUIRE( checkProcessed );
        REQUIRE( getChargePointStatus() == ChargePointStatus_Charging );
        REQUIRE( isOperative() );

#if MO_REPORT_NOERROR
        checkProcessed = false;
        getOcppContext()->getOperationRegistry().registerOperation("StatusNotification",
            [&checkProcessed] () {
                return new Ocpp16::CustomOperation("StatusNotification",
                    [ &checkProcessed] (JsonObject payload) {
                        //process req
                        checkProcessed = true;
                        REQUIRE( !strcmp(payload["errorCode"] | "_Undefined", "NoError") );
                        REQUIRE( !payload.containsKey("info") );
                        REQUIRE( !strcmp(payload["status"] | "_Undefined", "Charging") );
                    },
                    [] () {
                        //create conf
                        return createEmptyDocument();
                    });
            });
#else
        checkProcessed = true;
#endif //MO_REPORT_NOERROR

        errorCondition = false;

        loop();

        REQUIRE( checkProcessed );
        REQUIRE( getChargePointStatus() == ChargePointStatus_Charging );
        REQUIRE( isOperative() );

    }

    SECTION("Err and resolve (fatal)") {

        bool errorCondition = false;

        addErrorCodeInput([&errorCondition] () {
            return errorCondition ? "OtherError" : nullptr;
        });

        loop();

        REQUIRE( getChargePointStatus() == ChargePointStatus_Available );
        REQUIRE( isOperative() );

        errorCondition = true;

        loop();

        REQUIRE( getChargePointStatus() == ChargePointStatus_Faulted );
        REQUIRE( !isOperative() );

        errorCondition = false;

        loop();

        REQUIRE( getChargePointStatus() == ChargePointStatus_Available );
        REQUIRE( isOperative() );
    }

    SECTION("Error severity") {

        bool errorConditionLow1 = false;
        bool errorConditionLow2 = false;
        bool errorConditionHigh = false;

        addErrorDataInput([&errorConditionLow1] () -> ErrorData {
            if (errorConditionLow1) {
                ErrorData error = "OtherError";
                error.severity = 1;
                error.info = ERROR_INFO_LOW_1;
                return error;
            }
            return nullptr;
        });

        addErrorDataInput([&errorConditionLow2] () -> ErrorData {
            if (errorConditionLow2) {
                ErrorData error = "OtherError";
                error.severity = 1;
                error.info = ERROR_INFO_LOW_2;
                return error;
            }
            return nullptr;
        });

        addErrorDataInput([&errorConditionHigh] () -> ErrorData {
            if (errorConditionHigh) {
                ErrorData error = "OtherError";
                error.severity = 2;
                error.info = ERROR_INFO_HIGH;
                return error;
            }
            return nullptr;
        });

        bool checkProcessed = false;
        const char *errorLabel = "";

        getOcppContext()->getOperationRegistry().registerOperation("StatusNotification",
            [&checkProcessed, &errorLabel] () {
                return new Ocpp16::CustomOperation("StatusNotification",
                    [&checkProcessed, &errorLabel] (JsonObject payload) {
                        //process req
                        if (payload.containsKey("info")) {
                            checkProcessed = true;
                            REQUIRE( !strcmp(payload["info"] | "_Undefined", errorLabel) );
                        }
                    },
                    [] () {
                        //create conf
                        return createEmptyDocument();
                    });
            });
        
        //sequence: low-level error 1, low-level error 2, then severe error  -- all errors should go through

        errorConditionLow1 = true;
        errorLabel = ERROR_INFO_LOW_1;
        loop();
        REQUIRE( checkProcessed );
        checkProcessed = false;
        
        errorConditionLow2 = true;
        errorLabel = ERROR_INFO_LOW_2;
        loop();
        REQUIRE( checkProcessed );
        checkProcessed = false;
        
        errorConditionHigh = true;
        errorLabel = ERROR_INFO_HIGH;
        loop();
        REQUIRE( checkProcessed );
        checkProcessed = false;

        errorConditionLow1 = false;
        errorConditionLow2 = false;
        errorConditionHigh = false;
        loop();
        
        //sequence: low-level error 1, severe error, then low-level error 2 -- last error gets muted until severe error is resolved

        errorConditionLow1 = true;
        errorLabel = ERROR_INFO_LOW_1;
        loop();
        REQUIRE( checkProcessed );
        checkProcessed = false;
        
        errorConditionHigh = true;
        errorLabel = ERROR_INFO_HIGH;
        loop();
        REQUIRE( checkProcessed );
        checkProcessed = false;
        
        errorConditionLow2 = true;
        errorLabel = ERROR_INFO_LOW_2;
        loop();
        REQUIRE( !checkProcessed );

        errorConditionHigh = false;
        loop();
        REQUIRE( checkProcessed );
        checkProcessed = false;
    }

    endTransaction();
    mocpp_deinitialize();

}
