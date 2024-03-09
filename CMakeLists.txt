# matth-x/MicroOcpp
# Copyright Matthias Akstaller 2019 - 2023
# MIT License

cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 11)

set(MO_SRC
    src/MicroOcpp/Core/Configuration.cpp
    src/MicroOcpp/Core/ConfigurationContainer.cpp
    src/MicroOcpp/Core/ConfigurationContainerFlash.cpp
    src/MicroOcpp/Core/ConfigurationKeyValue.cpp
    src/MicroOcpp/Core/FilesystemAdapter.cpp
    src/MicroOcpp/Core/FilesystemUtils.cpp
    src/MicroOcpp/Core/RequestQueue.cpp
    src/MicroOcpp/Core/Context.cpp
    src/MicroOcpp/Core/Operation.cpp
    src/MicroOcpp/Model/Model.cpp
    src/MicroOcpp/Core/Request.cpp
    src/MicroOcpp/Core/Connection.cpp
    src/MicroOcpp/Core/Time.cpp
    src/MicroOcpp/Core/RequestQueueStorageStrategy.cpp
    src/MicroOcpp/Core/RequestStore.cpp
    src/MicroOcpp/Operations/Authorize.cpp
    src/MicroOcpp/Operations/BootNotification.cpp
    src/MicroOcpp/Operations/CancelReservation.cpp
    src/MicroOcpp/Operations/ChangeAvailability.cpp
    src/MicroOcpp/Operations/ChangeConfiguration.cpp
    src/MicroOcpp/Operations/ClearCache.cpp
    src/MicroOcpp/Operations/ClearChargingProfile.cpp
    src/MicroOcpp/Operations/CustomOperation.cpp
    src/MicroOcpp/Operations/DataTransfer.cpp
    src/MicroOcpp/Operations/DeleteCertificate.cpp
    src/MicroOcpp/Operations/DiagnosticsStatusNotification.cpp
    src/MicroOcpp/Operations/FirmwareStatusNotification.cpp
    src/MicroOcpp/Operations/GetCompositeSchedule.cpp
    src/MicroOcpp/Operations/GetConfiguration.cpp
    src/MicroOcpp/Operations/GetDiagnostics.cpp
    src/MicroOcpp/Operations/GetInstalledCertificateIds.cpp
    src/MicroOcpp/Operations/GetLocalListVersion.cpp
    src/MicroOcpp/Operations/GetVariables.cpp
    src/MicroOcpp/Operations/Heartbeat.cpp
    src/MicroOcpp/Operations/MeterValues.cpp
    src/MicroOcpp/Operations/RemoteStartTransaction.cpp
    src/MicroOcpp/Operations/RemoteStopTransaction.cpp
    src/MicroOcpp/Operations/ReserveNow.cpp
    src/MicroOcpp/Operations/Reset.cpp
    src/MicroOcpp/Operations/SendLocalList.cpp
    src/MicroOcpp/Operations/SetChargingProfile.cpp
    src/MicroOcpp/Operations/SetVariables.cpp
    src/MicroOcpp/Operations/StartTransaction.cpp
    src/MicroOcpp/Operations/StatusNotification.cpp
    src/MicroOcpp/Operations/StopTransaction.cpp
    src/MicroOcpp/Operations/TransactionEvent.cpp
    src/MicroOcpp/Operations/TriggerMessage.cpp
    src/MicroOcpp/Operations/InstallCertificate.cpp
    src/MicroOcpp/Operations/UnlockConnector.cpp
    src/MicroOcpp/Operations/UpdateFirmware.cpp
    src/MicroOcpp/Platform.cpp
    src/MicroOcpp/Core/SimpleRequestFactory.cpp
    src/MicroOcpp/Core/OperationRegistry.cpp
    src/MicroOcpp/Model/Authorization/AuthorizationData.cpp
    src/MicroOcpp/Model/Authorization/AuthorizationList.cpp
    src/MicroOcpp/Model/Authorization/AuthorizationService.cpp
    src/MicroOcpp/Model/Authorization/IdToken.cpp
    src/MicroOcpp/Model/Boot/BootService.cpp
    src/MicroOcpp/Model/Certificates/Certificate.cpp
    src/MicroOcpp/Model/Certificates/Certificate_c.cpp
    src/MicroOcpp/Model/Certificates/CertificateMbedTLS.cpp
    src/MicroOcpp/Model/Certificates/CertificateService.cpp
    src/MicroOcpp/Model/ConnectorBase/ConnectorsCommon.cpp
    src/MicroOcpp/Model/ConnectorBase/Connector.cpp
    src/MicroOcpp/Model/ConnectorBase/Notification.cpp
    src/MicroOcpp/Model/Diagnostics/DiagnosticsService.cpp
    src/MicroOcpp/Model/FirmwareManagement/FirmwareService.cpp
    src/MicroOcpp/Model/Heartbeat/HeartbeatService.cpp
    src/MicroOcpp/Model/Metering/MeteringConnector.cpp
    src/MicroOcpp/Model/Metering/MeteringService.cpp
    src/MicroOcpp/Model/Metering/MeterStore.cpp
    src/MicroOcpp/Model/Metering/MeterValue.cpp
    src/MicroOcpp/Model/Metering/SampledValue.cpp
    src/MicroOcpp/Model/Reservation/Reservation.cpp
    src/MicroOcpp/Model/Reservation/ReservationService.cpp
    src/MicroOcpp/Model/Reset/ResetService.cpp
    src/MicroOcpp/Model/SmartCharging/SmartChargingModel.cpp
    src/MicroOcpp/Model/SmartCharging/SmartChargingService.cpp
    src/MicroOcpp/Model/Transactions/Transaction.cpp
    src/MicroOcpp/Model/Transactions/TransactionDeserialize.cpp
    src/MicroOcpp/Model/Transactions/TransactionService.cpp
    src/MicroOcpp/Model/Transactions/TransactionStore.cpp
    src/MicroOcpp/Model/Variables/Variable.cpp
    src/MicroOcpp/Model/Variables/VariableContainer.cpp
    src/MicroOcpp/Model/Variables/VariableService.cpp
    src/MicroOcpp.cpp
    src/MicroOcpp_c.cpp
)

if(ESP_PLATFORM)

    idf_component_register(SRCS ${MO_SRC}
                        INCLUDE_DIRS "./src" "../ArduinoJson/src"
                        PRIV_REQUIRES spiffs)

    target_compile_options(${COMPONENT_TARGET} PUBLIC
        -DMO_PLATFORM=MO_PLATFORM_ESPIDF
        -DMO_CUSTOM_WS
        -DMO_CUSTOM_UPDATER
        -DMO_CUSTOM_RESET
        )

    return()
endif()

project(MicroOcpp
        VERSION 0.2.0)

add_library(MicroOcpp ${MO_SRC})

target_include_directories(MicroOcpp PUBLIC
    "./src"
    "../ArduinoJson/src"
)

target_compile_definitions(MicroOcpp PUBLIC
    MO_PLATFORM=MO_PLATFORM_UNIX
    MO_CUSTOM_WS
    MO_CUSTOM_UPDATER
    MO_CUSTOM_RESET
)

# Unit tests

set(MO_SRC_UNIT
    tests/helpers/testHelper.cpp
    tests/ocppEngineLifecycle.cpp
    tests/TransactionSafety.cpp
    tests/ChargingSessions.cpp
    tests/ConfigurationBehavior.cpp
    tests/SmartCharging.cpp
    tests/Api.cpp
    tests/Metering.cpp
    tests/Configuration.cpp
    tests/Reservation.cpp
    tests/LocalAuthList.cpp
    tests/Variables.cpp
    tests/Transactions.cpp
    #tests/Certificates.cpp # add if MbedTLS is available
)

add_executable(mo_unit_tests
    ${MO_SRC}
    ${MO_SRC_UNIT}
    ./tests/catch2/catchMain.cpp
)

# add MbedTLS for testing (TODO integrate properly into build system)
#add_subdirectory(lib/mbedtls)
#target_link_libraries(mo_unit_tests PUBLIC 
#    mbedtls
#    mbedcrypto
#    mbedx509
#)

target_include_directories(mo_unit_tests PUBLIC
    "./tests/catch2"
    "./tests/helpers"
    "./src"
)

target_compile_definitions(mo_unit_tests PUBLIC
    MO_PLATFORM=MO_PLATFORM_UNIX
    MO_NUMCONNECTORS=3
    MO_CUSTOM_TIMER
    MO_CUSTOM_WS
    MO_CUSTOM_UPDATER
    MO_CUSTOM_RESET
    MO_DBG_LEVEL=MO_DL_DEBUG
    MO_TRAFFIC_OUT
    MO_FILENAME_PREFIX="./mo_store/"
    MO_LocalAuthListMaxLength=8
    MO_SendLocalListMaxLength=4
    MO_ENABLE_FILE_INDEX=1
    #MO_ENABLE_MBEDTLS=1
)

target_compile_options(mo_unit_tests PUBLIC
    -O0
    -g
    --coverage
)

target_link_options(mo_unit_tests PUBLIC
    --coverage
)
