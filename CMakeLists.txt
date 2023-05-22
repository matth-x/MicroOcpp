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
    src/ArduinoOcpp/Core/OcppConnection.cpp
    src/ArduinoOcpp/Core/OcppEngine.cpp
    src/ArduinoOcpp/Core/OcppMessage.cpp
    src/ArduinoOcpp/Core/OcppModel.cpp
    src/ArduinoOcpp/Core/OcppOperation.cpp
    src/ArduinoOcpp/Core/OcppSocket.cpp
    src/ArduinoOcpp/Core/OcppTime.cpp
    src/ArduinoOcpp/Core/OperationsQueue.cpp
    src/ArduinoOcpp/Core/OperationStore.cpp
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
    src/ArduinoOcpp/SimpleOcppOperationFactory.cpp
    src/ArduinoOcpp/OperationDeserializer.cpp
    src/ArduinoOcpp/Tasks/Authorization/AuthorizationData.cpp
    src/ArduinoOcpp/Tasks/Authorization/AuthorizationList.cpp
    src/ArduinoOcpp/Tasks/Authorization/AuthorizationService.cpp
    src/ArduinoOcpp/Tasks/Boot/BootService.cpp
    src/ArduinoOcpp/Tasks/ChargeControl/ChargeControlService.cpp
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
