import numpy as np
import pandas as pd

# load data

modules = pd.read_csv('docs/assets/bloaty_v16.csv', index_col="compileunits").filter(like="lib/MicroOcpp", axis=0).filter(['Module','v16','v201','vmsize'], axis=1).sort_index()
modules.index.names = ['Compile Unit']
COLUMN_BINSIZE = 'Binary size (Bytes)'
modules.rename(columns={'vmsize': COLUMN_BINSIZE}, inplace=True)

# categorize data

modules["v16"] = ''
modules["v201"] = ''
modules["Module"] = ''

TICK = 'x'

MODULE_GENERAL = 'General'
MODULE_HAL = 'General - HAL'
MODULE_TESTS = 'General - Testing'
MODULE_RPC = 'General - RPC framework'
MODULE_CORE = 'Core'
MODULE_CONFIGURATION = 'Configuration'
MODULE_FW_MNGT = 'Firmware Management'
MODULE_TRIGGERMESSAGE = 'TriggerMessage'
MODULE_SECURITY = 'A - Security'
MODULE_PROVISIONING = 'B - Provisioning'
MODULE_PROVISIONING_VARS = 'B - Provisioning - Variables'
MODULE_AUTHORIZATION = 'C - Authorization'
MODULE_LOCALAUTH = 'D - Local Authorization List Management'
MODULE_TX = 'E - Transactions'
MODULE_AVAILABILITY = 'G - Availability'
MODULE_RESERVATION = 'H - Reservation'
MODULE_METERVALUES = 'J - MeterValues'
MODULE_SMARTCHARGING = 'K - SmartCharging'
MODULE_CERTS = 'M - Certificate Management'

modules.at['lib/MicroOcpp.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp.cpp', 'Module'] = MODULE_GENERAL
modules.at['lib/MicroOcpp/Core/Configuration.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Core/Configuration.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Core/Configuration.cpp', 'Module'] = MODULE_CONFIGURATION
modules.at['lib/MicroOcpp/Core/Configuration_c.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Core/Configuration_c.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Core/Configuration_c.cpp', 'Module'] = MODULE_CONFIGURATION
modules.at['lib/MicroOcpp/Core/ConfigurationContainer.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Core/ConfigurationContainer.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Core/ConfigurationContainer.cpp', 'Module'] = MODULE_CONFIGURATION
modules.at['lib/MicroOcpp/Core/ConfigurationContainerFlash.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Core/ConfigurationContainerFlash.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Core/ConfigurationContainerFlash.cpp', 'Module'] = MODULE_CONFIGURATION
modules.at['lib/MicroOcpp/Core/ConfigurationKeyValue.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Core/ConfigurationKeyValue.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Core/ConfigurationKeyValue.cpp', 'Module'] = MODULE_CONFIGURATION
modules.at['lib/MicroOcpp/Core/Connection.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Core/Connection.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Core/Connection.cpp', 'Module'] = MODULE_HAL
modules.at['lib/MicroOcpp/Core/Context.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Core/Context.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Core/Context.cpp', 'Module'] = MODULE_GENERAL
modules.at['lib/MicroOcpp/Core/FilesystemAdapter.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Core/FilesystemAdapter.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Core/FilesystemAdapter.cpp', 'Module'] = MODULE_HAL
modules.at['lib/MicroOcpp/Core/FilesystemUtils.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Core/FilesystemUtils.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Core/FilesystemUtils.cpp', 'Module'] = MODULE_GENERAL
modules.at['lib/MicroOcpp/Core/Memory.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Core/Memory.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Core/Memory.cpp', 'Module'] = MODULE_TESTS
modules.at['lib/MicroOcpp/Core/Operation.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Core/Operation.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Core/Operation.cpp', 'Module'] = MODULE_RPC
modules.at['lib/MicroOcpp/Core/OperationRegistry.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Core/OperationRegistry.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Core/OperationRegistry.cpp', 'Module'] = MODULE_RPC
modules.at['lib/MicroOcpp/Core/Request.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Core/Request.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Core/Request.cpp', 'Module'] = MODULE_RPC
modules.at['lib/MicroOcpp/Core/RequestQueue.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Core/RequestQueue.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Core/RequestQueue.cpp', 'Module'] = MODULE_RPC
modules.at['lib/MicroOcpp/Core/Time.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Core/Time.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Core/Time.cpp', 'Module'] = MODULE_GENERAL
modules.at['lib/MicroOcpp/Debug.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Debug.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Debug.cpp', 'Module'] = MODULE_HAL
modules.at['lib/MicroOcpp/Model/Authorization/AuthorizationData.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/Authorization/AuthorizationData.cpp', 'Module'] = MODULE_LOCALAUTH
modules.at['lib/MicroOcpp/Model/Authorization/AuthorizationList.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/Authorization/AuthorizationList.cpp', 'Module'] = MODULE_LOCALAUTH
modules.at['lib/MicroOcpp/Model/Authorization/AuthorizationService.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/Authorization/AuthorizationService.cpp', 'Module'] = MODULE_LOCALAUTH
modules.at['lib/MicroOcpp/Model/Authorization/IdToken.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Model/Authorization/IdToken.cpp', 'Module'] = MODULE_AUTHORIZATION
modules.at['lib/MicroOcpp/Model/Availability/AvailabilityService.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/Availability/AvailabilityService.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Model/Availability/AvailabilityService.cpp', 'Module'] = MODULE_TX
modules.at['lib/MicroOcpp/Model/Boot/BootService.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/Boot/BootService.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Model/Boot/BootService.cpp', 'Module'] = MODULE_PROVISIONING
modules.at['lib/MicroOcpp/Model/Certificates/Certificate_c.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/Certificates/Certificate_c.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Model/Certificates/Certificate_c.cpp', 'Module'] = MODULE_CERTS
modules.at['lib/MicroOcpp/Model/Certificates/CertificateService.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/Certificates/CertificateService.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Model/Certificates/CertificateService.cpp', 'Module'] = MODULE_CERTS
modules.at['lib/MicroOcpp/Model/ConnectorBase/Connector.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/ConnectorBase/Connector.cpp', 'Module'] = MODULE_CORE
modules.at['lib/MicroOcpp/Model/ConnectorBase/ConnectorsCommon.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/ConnectorBase/ConnectorsCommon.cpp', 'Module'] = MODULE_CORE
modules.at['lib/MicroOcpp/Model/Diagnostics/DiagnosticsService.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/Diagnostics/DiagnosticsService.cpp', 'Module'] = MODULE_FW_MNGT
modules.at['lib/MicroOcpp/Model/FirmwareManagement/FirmwareService.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/FirmwareManagement/FirmwareService.cpp', 'Module'] = MODULE_FW_MNGT
modules.at['lib/MicroOcpp/Model/Heartbeat/HeartbeatService.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/Heartbeat/HeartbeatService.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Model/Heartbeat/HeartbeatService.cpp', 'Module'] = MODULE_AVAILABILITY
modules.at['lib/MicroOcpp/Model/Metering/MeteringConnector.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/Metering/MeteringConnector.cpp', 'Module'] = MODULE_METERVALUES
modules.at['lib/MicroOcpp/Model/Metering/MeteringService.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/Metering/MeteringService.cpp', 'Module'] = MODULE_METERVALUES
modules.at['lib/MicroOcpp/Model/Metering/MeterStore.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/Metering/MeterStore.cpp', 'Module'] = MODULE_METERVALUES
modules.at['lib/MicroOcpp/Model/Metering/MeterValue.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/Metering/MeterValue.cpp', 'Module'] = MODULE_METERVALUES
modules.at['lib/MicroOcpp/Model/Metering/SampledValue.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/Metering/SampledValue.cpp', 'Module'] = MODULE_METERVALUES
modules.at['lib/MicroOcpp/Model/Model.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/Model.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Model/Model.cpp', 'Module'] = MODULE_GENERAL
modules.at['lib/MicroOcpp/Model/Reservation/Reservation.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/Reservation/Reservation.cpp', 'Module'] = MODULE_RESERVATION
modules.at['lib/MicroOcpp/Model/Reservation/ReservationService.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/Reservation/ReservationService.cpp', 'Module'] = MODULE_RESERVATION
modules.at['lib/MicroOcpp/Model/Reset/ResetService.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/Reset/ResetService.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Model/Reset/ResetService.cpp', 'Module'] = MODULE_PROVISIONING
modules.at['lib/MicroOcpp/Model/SmartCharging/SmartChargingModel.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/SmartCharging/SmartChargingModel.cpp', 'Module'] = MODULE_SMARTCHARGING
modules.at['lib/MicroOcpp/Model/SmartCharging/SmartChargingService.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/SmartCharging/SmartChargingService.cpp', 'Module'] = MODULE_SMARTCHARGING
modules.at['lib/MicroOcpp/Model/Transactions/Transaction.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/Transactions/Transaction.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Model/Transactions/Transaction.cpp', 'Module'] = MODULE_TX
modules.at['lib/MicroOcpp/Model/Transactions/TransactionDeserialize.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/Transactions/TransactionDeserialize.cpp', 'Module'] = MODULE_TX
modules.at['lib/MicroOcpp/Model/Transactions/TransactionService.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Model/Transactions/TransactionService.cpp', 'Module'] = MODULE_TX
modules.at['lib/MicroOcpp/Model/Transactions/TransactionStore.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Model/Transactions/TransactionStore.cpp', 'Module'] = MODULE_TX
modules.at['lib/MicroOcpp/Model/Variables/Variable.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Model/Variables/Variable.cpp', 'Module'] = MODULE_PROVISIONING_VARS
modules.at['lib/MicroOcpp/Model/Variables/VariableContainer.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Model/Variables/VariableContainer.cpp', 'Module'] = MODULE_PROVISIONING_VARS
modules.at['lib/MicroOcpp/Model/Variables/VariableService.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Model/Variables/VariableService.cpp', 'Module'] = MODULE_PROVISIONING_VARS
modules.at['lib/MicroOcpp/Operations/Authorize.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/Authorize.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Operations/Authorize.cpp', 'Module'] = MODULE_AUTHORIZATION
modules.at['lib/MicroOcpp/Operations/BootNotification.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/BootNotification.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Operations/BootNotification.cpp', 'Module'] = MODULE_PROVISIONING
modules.at['lib/MicroOcpp/Operations/CancelReservation.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/CancelReservation.cpp', 'Module'] = MODULE_RESERVATION
modules.at['lib/MicroOcpp/Operations/ChangeAvailability.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/ChangeAvailability.cpp', 'Module'] = MODULE_AVAILABILITY
modules.at['lib/MicroOcpp/Operations/ChangeConfiguration.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/ChangeConfiguration.cpp', 'Module'] = MODULE_CONFIGURATION
modules.at['lib/MicroOcpp/Operations/ClearCache.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/ClearCache.cpp', 'Module'] = MODULE_CORE
modules.at['lib/MicroOcpp/Operations/ClearChargingProfile.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/ClearChargingProfile.cpp', 'Module'] = MODULE_SMARTCHARGING
modules.at['lib/MicroOcpp/Operations/CustomOperation.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/CustomOperation.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Operations/CustomOperation.cpp', 'Module'] = MODULE_RPC
modules.at['lib/MicroOcpp/Operations/DataTransfer.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/DataTransfer.cpp', 'Module'] = MODULE_CORE
modules.at['lib/MicroOcpp/Operations/DeleteCertificate.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/DeleteCertificate.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Operations/DeleteCertificate.cpp', 'Module'] = MODULE_CERTS
modules.at['lib/MicroOcpp/Operations/DiagnosticsStatusNotification.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/DiagnosticsStatusNotification.cpp', 'Module'] = MODULE_FW_MNGT
modules.at['lib/MicroOcpp/Operations/FirmwareStatusNotification.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/FirmwareStatusNotification.cpp', 'Module'] = MODULE_FW_MNGT
modules.at['lib/MicroOcpp/Operations/GetBaseReport.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Operations/GetBaseReport.cpp', 'Module'] = MODULE_PROVISIONING_VARS
modules.at['lib/MicroOcpp/Operations/GetCompositeSchedule.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/GetCompositeSchedule.cpp', 'Module'] = MODULE_SMARTCHARGING
modules.at['lib/MicroOcpp/Operations/GetConfiguration.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/GetConfiguration.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Operations/GetConfiguration.cpp', 'Module'] = MODULE_CONFIGURATION
modules.at['lib/MicroOcpp/Operations/GetDiagnostics.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/GetDiagnostics.cpp', 'Module'] = MODULE_FW_MNGT
modules.at['lib/MicroOcpp/Operations/GetInstalledCertificateIds.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/GetInstalledCertificateIds.cpp', 'Module'] = MODULE_SMARTCHARGING
modules.at['lib/MicroOcpp/Operations/GetLocalListVersion.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/GetLocalListVersion.cpp', 'Module'] = MODULE_LOCALAUTH
modules.at['lib/MicroOcpp/Operations/GetVariables.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Operations/GetVariables.cpp', 'Module'] = MODULE_PROVISIONING_VARS
modules.at['lib/MicroOcpp/Operations/Heartbeat.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/Heartbeat.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Operations/Heartbeat.cpp', 'Module'] = MODULE_AVAILABILITY
modules.at['lib/MicroOcpp/Operations/InstallCertificate.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/InstallCertificate.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Operations/InstallCertificate.cpp', 'Module'] = MODULE_CERTS
modules.at['lib/MicroOcpp/Operations/MeterValues.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/MeterValues.cpp', 'Module'] = MODULE_METERVALUES
modules.at['lib/MicroOcpp/Operations/NotifyReport.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Operations/NotifyReport.cpp', 'Module'] = MODULE_PROVISIONING_VARS
modules.at['lib/MicroOcpp/Operations/RemoteStartTransaction.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/RemoteStartTransaction.cpp', 'Module'] = MODULE_TX
modules.at['lib/MicroOcpp/Operations/RemoteStopTransaction.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/RemoteStopTransaction.cpp', 'Module'] = MODULE_TX
modules.at['lib/MicroOcpp/Operations/RequestStartTransaction.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Operations/RequestStartTransaction.cpp', 'Module'] = MODULE_TX
modules.at['lib/MicroOcpp/Operations/RequestStopTransaction.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Operations/RequestStopTransaction.cpp', 'Module'] = MODULE_TX
modules.at['lib/MicroOcpp/Operations/ReserveNow.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/ReserveNow.cpp', 'Module'] = MODULE_RESERVATION
modules.at['lib/MicroOcpp/Operations/Reset.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/Reset.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Operations/Reset.cpp', 'Module'] = MODULE_PROVISIONING
modules.at['lib/MicroOcpp/Operations/SecurityEventNotification.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Operations/SecurityEventNotification.cpp', 'Module'] = MODULE_SECURITY
modules.at['lib/MicroOcpp/Operations/SendLocalList.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/SendLocalList.cpp', 'Module'] = MODULE_LOCALAUTH
modules.at['lib/MicroOcpp/Operations/SetChargingProfile.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/SetChargingProfile.cpp', 'Module'] = MODULE_SMARTCHARGING
modules.at['lib/MicroOcpp/Operations/SetVariables.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Operations/SetVariables.cpp', 'Module'] = MODULE_PROVISIONING_VARS
modules.at['lib/MicroOcpp/Operations/StartTransaction.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/StartTransaction.cpp', 'Module'] = MODULE_TX
modules.at['lib/MicroOcpp/Operations/StatusNotification.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/StatusNotification.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Operations/StatusNotification.cpp', 'Module'] = MODULE_AVAILABILITY
modules.at['lib/MicroOcpp/Operations/StopTransaction.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/StopTransaction.cpp', 'Module'] = MODULE_TX
modules.at['lib/MicroOcpp/Operations/TransactionEvent.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp/Operations/TransactionEvent.cpp', 'Module'] = MODULE_TX
modules.at['lib/MicroOcpp/Operations/TriggerMessage.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/TriggerMessage.cpp', 'Module'] = MODULE_TRIGGERMESSAGE
modules.at['lib/MicroOcpp/Operations/UnlockConnector.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/UnlockConnector.cpp', 'Module'] = MODULE_CORE
modules.at['lib/MicroOcpp/Operations/UpdateFirmware.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp/Operations/UpdateFirmware.cpp', 'Module'] = MODULE_FW_MNGT
modules.at['lib/MicroOcpp_c.cpp', 'v16'] = TICK
modules.at['lib/MicroOcpp_c.cpp', 'v201'] = TICK
modules.at['lib/MicroOcpp_c.cpp', 'Module'] = MODULE_GENERAL

print(modules)

# store csv with all details

modules.to_csv("compile_units.csv")

print('Uncategorized compile units: ', (modules['Module'].values == '').sum())

# store csv with size by Module for v16

modules_v16 = modules.loc[modules['v16'].values == 'x'].sort_index()
modules_v16_by_module = modules_v16[['Module', COLUMN_BINSIZE]].groupby('Module').sum()
modules_v16_by_module.loc['**Total**'] = [modules_v16_by_module[COLUMN_BINSIZE].sum()]

print(modules_v16_by_module)

modules_v16_by_module.to_csv('modules_v16.csv')

# store csv with size by Module for v201

modules_v201 = modules.loc[modules['v201'].values == 'x'].sort_index()
modules_v201_by_module = modules_v201[['Module', COLUMN_BINSIZE]].groupby('Module').sum()
modules_v201_by_module.loc['**Total**'] = [modules_v201_by_module[COLUMN_BINSIZE].sum()]

print(modules_v201_by_module)

modules_v201_by_module.to_csv('modules_v16.csv')
