# matth-x/ArduinoOcpp
# Copyright Matthias Akstaller 2019 - 2023
# MIT License

cmake_minimum_required(VERSION 3.15)

set(AO_SRC
    src/ArduinoOcpp/Core/Configuration.cpp
    src/ArduinoOcpp/Core/ConfigurationContainer.cpp
    src/ArduinoOcpp/Core/ConfigurationContainerFlash.cpp
    src/ArduinoOcpp/Core/ConfigurationKeyValue.cpp
    src/ArduinoOcpp/Core/FilesystemAdapter.cpp
    src/ArduinoOcpp/Core/FilesystemUtils.cpp
    src/ArduinoOcpp/Core/RequestQueue.cpp
    src/ArduinoOcpp/Core/Context.cpp
    src/ArduinoOcpp/Core/Operation.cpp
    src/ArduinoOcpp/Core/Model.cpp
    src/ArduinoOcpp/Core/Request.cpp
    src/ArduinoOcpp/Core/Connection.cpp
    src/ArduinoOcpp/Core/Time.cpp
    src/ArduinoOcpp/Core/RequestQueueStorageStrategy.cpp
    src/ArduinoOcpp/Core/RequestStore.cpp
    src/ArduinoOcpp/MessagesV16/Authorize.cpp
    src/ArduinoOcpp/MessagesV16/BootNotification.cpp
    src/ArduinoOcpp/MessagesV16/CancelReservation.cpp
    src/ArduinoOcpp/MessagesV16/ChangeAvailability.cpp
    src/ArduinoOcpp/MessagesV16/ChangeConfiguration.cpp
    src/ArduinoOcpp/MessagesV16/ClearCache.cpp
    src/ArduinoOcpp/MessagesV16/ClearChargingProfile.cpp
    src/ArduinoOcpp/MessagesV16/CustomMessage.cpp
    src/ArduinoOcpp/MessagesV16/DataTransfer.cpp
    src/ArduinoOcpp/MessagesV16/DiagnosticsStatusNotification.cpp
    src/ArduinoOcpp/MessagesV16/FirmwareStatusNotification.cpp
    src/ArduinoOcpp/MessagesV16/GetCompositeSchedule.cpp
    src/ArduinoOcpp/MessagesV16/GetConfiguration.cpp
    src/ArduinoOcpp/MessagesV16/GetDiagnostics.cpp
    src/ArduinoOcpp/MessagesV16/GetLocalListVersion.cpp
    src/ArduinoOcpp/MessagesV16/Heartbeat.cpp
    src/ArduinoOcpp/MessagesV16/MeterValues.cpp
    src/ArduinoOcpp/MessagesV16/RemoteStartTransaction.cpp
    src/ArduinoOcpp/MessagesV16/RemoteStopTransaction.cpp
    src/ArduinoOcpp/MessagesV16/ReserveNow.cpp
    src/ArduinoOcpp/MessagesV16/Reset.cpp
    src/ArduinoOcpp/MessagesV16/SendLocalList.cpp
    src/ArduinoOcpp/MessagesV16/SetChargingProfile.cpp
    src/ArduinoOcpp/MessagesV16/StartTransaction.cpp
    src/ArduinoOcpp/MessagesV16/StatusNotification.cpp
    src/ArduinoOcpp/MessagesV16/StopTransaction.cpp
    src/ArduinoOcpp/MessagesV16/TriggerMessage.cpp
    src/ArduinoOcpp/MessagesV16/UnlockConnector.cpp
    src/ArduinoOcpp/MessagesV16/UpdateFirmware.cpp
    src/ArduinoOcpp/Platform.cpp
    src/ArduinoOcpp/Core/SimpleRequestFactory.cpp
    src/ArduinoOcpp/Core/OperationRegistry.cpp
    src/ArduinoOcpp/Tasks/Authorization/AuthorizationData.cpp
    src/ArduinoOcpp/Tasks/Authorization/AuthorizationList.cpp
    src/ArduinoOcpp/Tasks/Authorization/AuthorizationService.cpp
    src/ArduinoOcpp/Tasks/Boot/BootService.cpp
    src/ArduinoOcpp/Tasks/ChargeControl/ChargeControlCommon.cpp
    src/ArduinoOcpp/Tasks/ChargeControl/Connector.cpp
    src/ArduinoOcpp/Tasks/Diagnostics/DiagnosticsService.cpp
    src/ArduinoOcpp/Tasks/FirmwareManagement/FirmwareService.cpp
    src/ArduinoOcpp/Tasks/Heartbeat/HeartbeatService.cpp
    src/ArduinoOcpp/Tasks/Metering/ConnectorMeterValuesRecorder.cpp
    src/ArduinoOcpp/Tasks/Metering/MeteringService.cpp
    src/ArduinoOcpp/Tasks/Metering/MeterStore.cpp
    src/ArduinoOcpp/Tasks/Metering/MeterValue.cpp
    src/ArduinoOcpp/Tasks/Metering/SampledValue.cpp
    src/ArduinoOcpp/Tasks/Reservation/Reservation.cpp
    src/ArduinoOcpp/Tasks/Reservation/ReservationService.cpp
    src/ArduinoOcpp/Tasks/Reset/ResetService.cpp
    src/ArduinoOcpp/Tasks/SmartCharging/SmartChargingModel.cpp
    src/ArduinoOcpp/Tasks/SmartCharging/SmartChargingService.cpp
    src/ArduinoOcpp/Tasks/Transactions/Transaction.cpp
    src/ArduinoOcpp/Tasks/Transactions/TransactionDeserialize.cpp
    src/ArduinoOcpp/Tasks/Transactions/TransactionStore.cpp
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
