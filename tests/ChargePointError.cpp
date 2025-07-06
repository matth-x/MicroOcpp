// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include <catch2/catch.hpp>
#include "./helpers/testHelper.h"

#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Operations/CustomOperation.h>
#include <MicroOcpp/Debug.h>

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

        getOcppContext()->getMessageService().registerOperation("StatusNotification",
            [&checkProcessed] () {
                return new v16::CustomOperation("StatusNotification",
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
        getOcppContext()->getMessageService().registerOperation("StatusNotification",
            [&checkProcessed] () {
                return new v16::CustomOperation("StatusNotification",
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

        const char *errorCode = "*";
        bool checkErrorCode = false;
        const char *errorInfo = "*";
        bool checkErrorInfo = false;

        getOcppContext()->getMessageService().registerOperation("StatusNotification",
            [&checkErrorCode, &checkErrorInfo, &errorInfo, &errorCode] () {
                return new v16::CustomOperation("StatusNotification",
                    [&checkErrorCode, &checkErrorInfo, &errorInfo, &errorCode] (JsonObject payload) {
                        //process req
                        if (strcmp(errorInfo, "*")) {
                            MO_DBG_DEBUG("expect \"%s\", got \"%s\"", errorInfo, payload["info"] | "_Undefined");
                            REQUIRE( !strcmp(payload["info"] | "_Undefined", errorInfo) );
                            checkErrorInfo = true;
                        }
                        if (strcmp(errorCode, "*")) {
                            MO_DBG_DEBUG("expect \"%s\", got \"%s\"", errorCode, payload["errorCode"] | "_Undefined");
                            REQUIRE( !strcmp(payload["errorCode"] | "_Undefined", errorCode) );
                            checkErrorCode = true;
                        }
                    },
                    [] () {
                        //create conf
                        return createEmptyDocument();
                    });
            });
        
        //sequence: low-level error 1, low-level error 2, then severe error  -- all errors should go through
        MO_DBG_INFO("test sequence: low-level error 1, low-level error 2, then severe error");

        errorConditionLow1 = true;
        errorInfo = ERROR_INFO_LOW_1;
        checkErrorInfo = false;
        loop();
        REQUIRE( checkErrorInfo );
        
        errorConditionLow2 = true;
        errorInfo = ERROR_INFO_LOW_2;
        checkErrorInfo = false;
        loop();
        REQUIRE( checkErrorInfo );
        
        errorConditionHigh = true;
        errorInfo = ERROR_INFO_HIGH;
        checkErrorInfo = false;
        loop();
        REQUIRE( checkErrorInfo );

        errorConditionLow1 = false;
        errorConditionLow2 = false;
        errorConditionHigh = false;
        errorInfo = "*";
        loop();
        
        //sequence: low-level error 1, severe error, then low-level error 2 -- last error gets muted until severe error is resolved
        MO_DBG_INFO("test sequence: low-level error 1, severe error, then low-level error 2");

        errorConditionLow1 = true;
        errorInfo = ERROR_INFO_LOW_1;
        checkErrorInfo = false;
        loop();
        REQUIRE( checkErrorInfo );
        
        errorConditionHigh = true;
        errorInfo = ERROR_INFO_HIGH;
        checkErrorInfo = false;
        loop();
        REQUIRE( checkErrorInfo );
        
        errorConditionLow2 = true;
        checkErrorInfo = false;
        loop();
        REQUIRE( !checkErrorInfo );

        errorConditionHigh = false;
        errorInfo = ERROR_INFO_LOW_2;
        checkErrorInfo = false;
        loop();
        REQUIRE( checkErrorInfo );

        errorConditionLow1 = false;
        errorConditionLow2 = false;
        errorConditionHigh = false;
        errorInfo = "*";
        loop();
        
        //sequence: low-level error 1, severe error, then severe error gets resolved -- low-level error is reported again
        MO_DBG_INFO("test sequence: low-level error 1, severe error, then severe error gets resolved");

        errorConditionLow1 = true;
        errorInfo = ERROR_INFO_LOW_1;
        checkErrorInfo = false;
        loop();
        REQUIRE( checkErrorInfo );
        
        errorConditionHigh = true;
        errorInfo = ERROR_INFO_HIGH;
        checkErrorInfo = false;
        loop();
        REQUIRE( checkErrorInfo );
        
        errorConditionHigh = false;
        errorInfo = ERROR_INFO_LOW_1;
        checkErrorInfo = false;
        loop();
        REQUIRE( checkErrorInfo );

        errorConditionLow1 = false;
        errorConditionLow2 = false;
        errorConditionHigh = false;
        errorInfo = "*";
        loop();

        //sequence: error, then error gets resolved -- report NoError
        MO_DBG_INFO("test sequence: error, then error gets resolved");

        errorConditionLow1 = true;
        errorInfo = ERROR_INFO_LOW_1;
        checkErrorInfo = false;
        loop();
        REQUIRE( checkErrorInfo );

        errorConditionLow1 = false;
        errorInfo = "*";
        errorCode = "NoError";
        checkErrorCode = false;
        loop();
        REQUIRE( checkErrorCode );
    }

    endTransaction();
    mocpp_deinitialize();

}
