# matth-x/MicroOcpp
# Copyright Matthias Akstaller 2019 - 2025
# MIT License

cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 11)

set(MO_SRC
    src/MicroOcpp/Core/Memory.cpp
    src/MicroOcpp/Core/Operation.cpp
    src/MicroOcpp/Core/FilesystemAdapter.cpp
    src/MicroOcpp/Core/PersistencyUtils.cpp
    src/MicroOcpp/Core/Request.cpp
    src/MicroOcpp/Core/FilesystemUtils.cpp
    src/MicroOcpp/Core/RequestQueue.cpp
    src/MicroOcpp/Core/MessageService.cpp
    src/MicroOcpp/Core/FtpMbedTLS.cpp
    src/MicroOcpp/Core/UuidUtils.cpp
    src/MicroOcpp/Core/Connection.cpp
    src/MicroOcpp/Core/Time.cpp
    src/MicroOcpp/Context.cpp
    src/MicroOcpp/Operations/GetVariables.cpp
    src/MicroOcpp/Operations/FirmwareStatusNotification.cpp
    src/MicroOcpp/Operations/GetInstalledCertificateIds.cpp
    src/MicroOcpp/Operations/UpdateFirmware.cpp
    src/MicroOcpp/Operations/MeterValues.cpp
    src/MicroOcpp/Operations/DataTransfer.cpp
    src/MicroOcpp/Operations/GetCompositeSchedule.cpp
    src/MicroOcpp/Operations/RemoteStartTransaction.cpp
    src/MicroOcpp/Operations/CustomOperation.cpp
    src/MicroOcpp/Operations/TransactionEvent.cpp
    src/MicroOcpp/Operations/ChangeAvailability.cpp
    src/MicroOcpp/Operations/RemoteStopTransaction.cpp
    src/MicroOcpp/Operations/ReserveNow.cpp
    src/MicroOcpp/Operations/CancelReservation.cpp
    src/MicroOcpp/Operations/TriggerMessage.cpp
    src/MicroOcpp/Operations/GetDiagnostics.cpp
    src/MicroOcpp/Operations/GetLocalListVersion.cpp
    src/MicroOcpp/Operations/RequestStartTransaction.cpp
    src/MicroOcpp/Operations/DeleteCertificate.cpp
    src/MicroOcpp/Operations/LogStatusNotification.cpp
    src/MicroOcpp/Operations/Heartbeat.cpp
    src/MicroOcpp/Operations/GetLog.cpp
    src/MicroOcpp/Operations/GetConfiguration.cpp
    src/MicroOcpp/Operations/ClearCache.cpp
    src/MicroOcpp/Operations/SetChargingProfile.cpp
    src/MicroOcpp/Operations/Authorize.cpp
    src/MicroOcpp/Operations/SecurityEventNotification.cpp
    src/MicroOcpp/Operations/RequestStopTransaction.cpp
    src/MicroOcpp/Operations/StatusNotification.cpp
    src/MicroOcpp/Operations/StopTransaction.cpp
    src/MicroOcpp/Operations/Reset.cpp
    src/MicroOcpp/Operations/SendLocalList.cpp
    src/MicroOcpp/Operations/DiagnosticsStatusNotification.cpp
    src/MicroOcpp/Operations/NotifyReport.cpp
    src/MicroOcpp/Operations/GetBaseReport.cpp
    src/MicroOcpp/Operations/ChangeConfiguration.cpp
    src/MicroOcpp/Operations/StartTransaction.cpp
    src/MicroOcpp/Operations/BootNotification.cpp
    src/MicroOcpp/Operations/SetVariables.cpp
    src/MicroOcpp/Operations/ClearChargingProfile.cpp
    src/MicroOcpp/Operations/UnlockConnector.cpp
    src/MicroOcpp/Operations/InstallCertificate.cpp
    src/MicroOcpp/Debug.cpp
    src/MicroOcpp/Model/Configuration/ConfigurationContainer.cpp
    src/MicroOcpp/Model/Configuration/Configuration.cpp
    src/MicroOcpp/Model/Configuration/ConfigurationService.cpp
    src/MicroOcpp/Model/Boot/BootService.cpp
    src/MicroOcpp/Model/SmartCharging/SmartChargingService.cpp
    src/MicroOcpp/Model/SmartCharging/SmartChargingModel.cpp
    src/MicroOcpp/Model/Reset/ResetService.cpp
    src/MicroOcpp/Model/Reservation/ReservationService.cpp
    src/MicroOcpp/Model/Reservation/Reservation.cpp
    src/MicroOcpp/Model/SecurityEvent/SecurityEventService.cpp
    src/MicroOcpp/Model/Certificates/CertificateMbedTLS.cpp
    src/MicroOcpp/Model/Certificates/Certificate.cpp
    src/MicroOcpp/Model/Certificates/CertificateService.cpp
    src/MicroOcpp/Model/FirmwareManagement/FirmwareService.cpp
    src/MicroOcpp/Model/Heartbeat/HeartbeatService.cpp
    src/MicroOcpp/Model/Transactions/TransactionStore.cpp
    src/MicroOcpp/Model/Transactions/TransactionService201.cpp
    src/MicroOcpp/Model/Transactions/TransactionService16.cpp
    src/MicroOcpp/Model/Transactions/Transaction.cpp
    src/MicroOcpp/Model/Variables/VariableContainer.cpp
    src/MicroOcpp/Model/Variables/VariableService.cpp
    src/MicroOcpp/Model/Variables/Variable.cpp
    src/MicroOcpp/Model/Diagnostics/Diagnostics.cpp
    src/MicroOcpp/Model/Diagnostics/DiagnosticsService.cpp
    src/MicroOcpp/Model/Model.cpp
    src/MicroOcpp/Model/Availability/AvailabilityDefs.cpp
    src/MicroOcpp/Model/Availability/AvailabilityService.cpp
    src/MicroOcpp/Model/Metering/MeteringService.cpp
    src/MicroOcpp/Model/Metering/ReadingContext.cpp
    src/MicroOcpp/Model/Metering/MeterStore.cpp
    src/MicroOcpp/Model/Metering/MeterValue.cpp
    src/MicroOcpp/Model/RemoteControl/RemoteControlService.cpp
    src/MicroOcpp/Model/Authorization/AuthorizationService.cpp
    src/MicroOcpp/Model/Authorization/AuthorizationData.cpp
    src/MicroOcpp/Model/Authorization/AuthorizationList.cpp
    src/MicroOcpp/Model/Authorization/IdToken.cpp
    src/MicroOcpp/Platform.cpp
    src/MicroOcpp.cpp
)

if(ESP_PLATFORM)

    idf_component_register(SRCS ${MO_SRC}
        INCLUDE_DIRS "./src" "../ArduinoJson/src"
        PRIV_REQUIRES spiffs
    )

    target_compile_options(${COMPONENT_TARGET} PUBLIC
        -DMO_PLATFORM=MO_PLATFORM_ESPIDF
    )

    return()
endif()

project(MicroOcpp VERSION 1.2.0)

add_library(MicroOcpp ${MO_SRC})

target_include_directories(MicroOcpp PUBLIC
    "./src"
    "../ArduinoJson/src"
)

target_compile_definitions(MicroOcpp PUBLIC
    MO_PLATFORM=MO_PLATFORM_UNIX
)

# Unit tests

set(MO_SRC_UNIT
    tests/helpers/testHelper.cpp
    tests/ocppEngineLifecycle.cpp
    tests/Core.cpp
    tests/TransactionSafety.cpp
    tests/ChargingSessions.cpp
    #tests/ConfigurationBehavior.cpp
    tests/SmartCharging.cpp
    #tests/Api.cpp
    #tests/Metering.cpp
    #tests/Configuration.cpp
    #tests/Reservation.cpp
    #tests/Reset.cpp
    #tests/LocalAuthList.cpp
    #tests/Variables.cpp
    #tests/Transactions.cpp
    #tests/Certificates.cpp
    #tests/FirmwareManagement.cpp
    #tests/ChargePointError.cpp
    #tests/Boot.cpp
    tests/Security.cpp
)

add_executable(mo_unit_tests
    ${MO_SRC}
    ${MO_SRC_UNIT}
    ./tests/catch2/catchMain.cpp
)

if (MO_BUILD_UNIT_MBEDTLS)
    add_subdirectory(lib/mbedtls)
    target_link_libraries(mo_unit_tests PUBLIC 
        mbedtls
        mbedcrypto
        mbedx509
    )

    target_compile_definitions(mo_unit_tests PUBLIC
        MO_ENABLE_MBEDTLS=1
    )
endif()

target_include_directories(mo_unit_tests PUBLIC
    "./tests"
    "./tests/helpers"
    "./src"
)

target_compile_definitions(mo_unit_tests PUBLIC
    MO_PLATFORM=MO_PLATFORM_UNIX
    MO_NUM_EVSEID=3
    MO_CUSTOM_TIMER
    MO_DBG_LEVEL=MO_DL_INFO
    MO_TRAFFIC_OUT
    MO_FILENAME_PREFIX="./mo_store/"
    MO_LocalAuthListMaxLength=8
    MO_SendLocalListMaxLength=4
    MO_ENABLE_FILE_INDEX=0
    MO_ENABLE_TIMESTAMP_MILLISECONDS=1
    MO_ChargeProfileMaxStackLevel=2
    MO_ChargingScheduleMaxPeriods=4
    MO_MaxChargingProfilesInstalled=3
    MO_ENABLE_CERT_MGMT=1
    MO_ENABLE_CONNECTOR_LOCK=1
    MO_REPORT_NOERROR=1
    MO_ENABLE_V201=1
    MO_OVERRIDE_ALLOCATION=1
    MO_ENABLE_HEAP_PROFILER=1
    MO_HEAP_PROFILER_EXTERNAL_CONTROL=1
    CATCH_CONFIG_EXTERNAL_INTERFACES
)

target_compile_options(mo_unit_tests PUBLIC
    -Wall
    -O0
    -g
    --coverage
)

target_link_options(mo_unit_tests PUBLIC
    --coverage
)
