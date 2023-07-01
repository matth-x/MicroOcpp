# matth-x/ArduinoOcpp
# Copyright Matthias Akstaller 2019 - 2023
# MIT License

cmake_minimum_required(VERSION 3.15)

project(ArduinoOcpp
        VERSION 0.2.0)

set(AO_SRC
    src/ArduinoOcpp/Core/Configuration.cpp
    src/ArduinoOcpp/Core/ConfigurationContainer.cpp
    src/ArduinoOcpp/Core/ConfigurationContainerFlash.cpp
    src/ArduinoOcpp/Core/ConfigurationKeyValue.cpp
    src/ArduinoOcpp/Core/FilesystemAdapter.cpp
    src/ArduinoOcpp/Core/FilesystemUtils.cpp
    src/ArduinoOcpp/Core/RequestQueue.cpp
    src/ArduinoOcpp/Context.cpp
    src/ArduinoOcpp/Core/Operation.cpp
    src/ArduinoOcpp/Model/Model.cpp
    src/ArduinoOcpp/Core/Request.cpp
    src/ArduinoOcpp/Core/Connection.cpp
    src/ArduinoOcpp/Core/Time.cpp
    src/ArduinoOcpp/Core/RequestQueueStorageStrategy.cpp
    src/ArduinoOcpp/Core/RequestStore.cpp
    src/ArduinoOcpp/Operations/Authorize.cpp
    src/ArduinoOcpp/Operations/BootNotification.cpp
    src/ArduinoOcpp/Operations/CancelReservation.cpp
    src/ArduinoOcpp/Operations/ChangeAvailability.cpp
    src/ArduinoOcpp/Operations/ChangeConfiguration.cpp
    src/ArduinoOcpp/Operations/ClearCache.cpp
    src/ArduinoOcpp/Operations/ClearChargingProfile.cpp
    src/ArduinoOcpp/Operations/CustomOperation.cpp
    src/ArduinoOcpp/Operations/DataTransfer.cpp
    src/ArduinoOcpp/Operations/DiagnosticsStatusNotification.cpp
    src/ArduinoOcpp/Operations/FirmwareStatusNotification.cpp
    src/ArduinoOcpp/Operations/GetCompositeSchedule.cpp
    src/ArduinoOcpp/Operations/GetConfiguration.cpp
    src/ArduinoOcpp/Operations/GetDiagnostics.cpp
    src/ArduinoOcpp/Operations/GetLocalListVersion.cpp
    src/ArduinoOcpp/Operations/Heartbeat.cpp
    src/ArduinoOcpp/Operations/MeterValues.cpp
    src/ArduinoOcpp/Operations/RemoteStartTransaction.cpp
    src/ArduinoOcpp/Operations/RemoteStopTransaction.cpp
    src/ArduinoOcpp/Operations/ReserveNow.cpp
    src/ArduinoOcpp/Operations/Reset.cpp
    src/ArduinoOcpp/Operations/SendLocalList.cpp
    src/ArduinoOcpp/Operations/SetChargingProfile.cpp
    src/ArduinoOcpp/Operations/StartTransaction.cpp
    src/ArduinoOcpp/Operations/StatusNotification.cpp
    src/ArduinoOcpp/Operations/StopTransaction.cpp
    src/ArduinoOcpp/Operations/TriggerMessage.cpp
    src/ArduinoOcpp/Operations/UnlockConnector.cpp
    src/ArduinoOcpp/Operations/UpdateFirmware.cpp
    src/ArduinoOcpp/Platform.cpp
    src/ArduinoOcpp/Core/SimpleRequestFactory.cpp
    src/ArduinoOcpp/Core/OperationRegistry.cpp
    src/ArduinoOcpp/Model/Authorization/AuthorizationData.cpp
    src/ArduinoOcpp/Model/Authorization/AuthorizationList.cpp
    src/ArduinoOcpp/Model/Authorization/AuthorizationService.cpp
    src/ArduinoOcpp/Model/Boot/BootService.cpp
    src/ArduinoOcpp/Model/ChargeControl/ChargeControlCommon.cpp
    src/ArduinoOcpp/Model/ChargeControl/Connector.cpp
    src/ArduinoOcpp/Model/ChargeControl/Notification.cpp
    src/ArduinoOcpp/Model/Diagnostics/DiagnosticsService.cpp
    src/ArduinoOcpp/Model/FirmwareManagement/FirmwareService.cpp
    src/ArduinoOcpp/Model/Heartbeat/HeartbeatService.cpp
    src/ArduinoOcpp/Model/Metering/MeteringConnector.cpp
    src/ArduinoOcpp/Model/Metering/MeteringService.cpp
    src/ArduinoOcpp/Model/Metering/MeterStore.cpp
    src/ArduinoOcpp/Model/Metering/MeterValue.cpp
    src/ArduinoOcpp/Model/Metering/SampledValue.cpp
    src/ArduinoOcpp/Model/Reservation/Reservation.cpp
    src/ArduinoOcpp/Model/Reservation/ReservationService.cpp
    src/ArduinoOcpp/Model/Reset/ResetService.cpp
    src/ArduinoOcpp/Model/SmartCharging/SmartChargingModel.cpp
    src/ArduinoOcpp/Model/SmartCharging/SmartChargingService.cpp
    src/ArduinoOcpp/Model/Transactions/Transaction.cpp
    src/ArduinoOcpp/Model/Transactions/TransactionDeserialize.cpp
    src/ArduinoOcpp/Model/Transactions/TransactionStore.cpp
    src/ArduinoOcpp.cpp
    src/ArduinoOcpp_c.cpp
)

if(ESP_PLATFORM)

    idf_component_register(SRCS ${AO_SRC}
                        INCLUDE_DIRS "./src" "../ArduinoJson/src"
                        PRIV_REQUIRES spiffs)

    target_compile_options(${COMPONENT_TARGET} PUBLIC
        -DAO_PLATFORM=AO_PLATFORM_ESPIDF
        -DAO_CUSTOM_WS
        -DAO_CUSTOM_UPDATER
        -DAO_CUSTOM_RESET
        )

    return()
endif()

add_library(ArduinoOcpp ${AO_SRC})

target_include_directories(ArduinoOcpp PUBLIC
    "./src"
    "../ArduinoJson/src"
)

target_compile_definitions(ArduinoOcpp PUBLIC
    AO_PLATFORM=AO_PLATFORM_UNIX
    AO_CUSTOM_WS
    AO_CUSTOM_UPDATER
    AO_CUSTOM_RESET
)

# Unit tests

set(AO_SRC_UNIT
    tests/helpers/testHelper.cpp
    tests/ocppEngineLifecycle.cpp
    tests/TransactionSafety.cpp
    tests/ChargingSessions.cpp
    tests/ConfigurationBehavior.cpp
    tests/SmartCharging.cpp
    tests/Api.cpp
)

add_executable(ao_unit_tests
    ${AO_SRC}
    ${AO_SRC_UNIT}
    ./tests/catch2/catchMain.cpp
)

target_include_directories(ao_unit_tests PUBLIC
    "./tests/catch2"
    "./tests/helpers"
    "./src"
)

target_compile_definitions(ao_unit_tests PUBLIC
    AO_PLATFORM=AO_PLATFORM_UNIX
    AO_CUSTOM_TIMER
    AO_CUSTOM_WS
    AO_CUSTOM_UPDATER
    AO_CUSTOM_RESET
    AO_DBG_LEVEL=AO_DL_DEBUG
    AO_TRAFFIC_OUT
    AO_FILENAME_PREFIX="./ao_store/"
)

target_compile_options(ao_unit_tests PUBLIC
    -O0
    -g
    --coverage
)

target_link_options(ao_unit_tests PUBLIC
    --coverage
)
