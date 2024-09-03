import numpy as np
import pandas as pd

# load data

COLUMN_BINSIZE = 'Binary size (Bytes)'

def load_compilation_units(fn):
    df = pd.read_csv(fn, index_col="compileunits").filter(like="lib/MicroOcpp/src/MicroOcpp", axis=0).filter(['Module','v16','v201','vmsize'], axis=1).sort_index()
    df.index.names = ['Compile Unit']
    df.rename(columns={'vmsize': COLUMN_BINSIZE}, inplace=True)
    return df
    
cunits_v16 = load_compilation_units('docs/assets/tables/bloaty_v16.csv')
cunits_v201 = load_compilation_units('docs/assets/tables/bloaty_v201.csv')

# categorize data

def categorize_table(df):

    df["v16"] = ''
    df["v201"] = ''
    df["Module"] = ''

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

    df.at['lib/MicroOcpp/src/MicroOcpp.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp.cpp', 'Module'] = MODULE_GENERAL
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Configuration.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Configuration.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Configuration.cpp', 'Module'] = MODULE_CONFIGURATION
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Configuration_c.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Configuration_c.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Configuration_c.cpp', 'Module'] = MODULE_CONFIGURATION
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/ConfigurationContainer.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/ConfigurationContainer.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/ConfigurationContainer.cpp', 'Module'] = MODULE_CONFIGURATION
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/ConfigurationContainerFlash.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/ConfigurationContainerFlash.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/ConfigurationContainerFlash.cpp', 'Module'] = MODULE_CONFIGURATION
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/ConfigurationKeyValue.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/ConfigurationKeyValue.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/ConfigurationKeyValue.cpp', 'Module'] = MODULE_CONFIGURATION
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Connection.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Connection.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Connection.cpp', 'Module'] = MODULE_HAL
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Context.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Context.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Context.cpp', 'Module'] = MODULE_GENERAL
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/FilesystemAdapter.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/FilesystemAdapter.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/FilesystemAdapter.cpp', 'Module'] = MODULE_HAL
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/FilesystemUtils.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/FilesystemUtils.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/FilesystemUtils.cpp', 'Module'] = MODULE_GENERAL
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Memory.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Memory.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Memory.cpp', 'Module'] = MODULE_TESTS
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Operation.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Operation.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Operation.cpp', 'Module'] = MODULE_RPC
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/OperationRegistry.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/OperationRegistry.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/OperationRegistry.cpp', 'Module'] = MODULE_RPC
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Request.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Request.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Request.cpp', 'Module'] = MODULE_RPC
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/RequestQueue.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/RequestQueue.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/RequestQueue.cpp', 'Module'] = MODULE_RPC
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Time.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Time.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Core/Time.cpp', 'Module'] = MODULE_GENERAL
    df.at['lib/MicroOcpp/src/MicroOcpp/Debug.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Debug.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Debug.cpp', 'Module'] = MODULE_HAL
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Authorization/AuthorizationData.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Authorization/AuthorizationData.cpp', 'Module'] = MODULE_LOCALAUTH
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Authorization/AuthorizationList.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Authorization/AuthorizationList.cpp', 'Module'] = MODULE_LOCALAUTH
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Authorization/AuthorizationService.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Authorization/AuthorizationService.cpp', 'Module'] = MODULE_LOCALAUTH
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Authorization/IdToken.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Authorization/IdToken.cpp', 'Module'] = MODULE_AUTHORIZATION
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Availability/AvailabilityService.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Availability/AvailabilityService.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Availability/AvailabilityService.cpp', 'Module'] = MODULE_TX
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Boot/BootService.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Boot/BootService.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Boot/BootService.cpp', 'Module'] = MODULE_PROVISIONING
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Certificates/Certificate_c.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Certificates/Certificate_c.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Certificates/Certificate_c.cpp', 'Module'] = MODULE_CERTS
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Certificates/CertificateService.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Certificates/CertificateService.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Certificates/CertificateService.cpp', 'Module'] = MODULE_CERTS
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/ConnectorBase/Connector.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/ConnectorBase/Connector.cpp', 'Module'] = MODULE_CORE
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/ConnectorBase/ConnectorsCommon.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/ConnectorBase/ConnectorsCommon.cpp', 'Module'] = MODULE_CORE
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Diagnostics/DiagnosticsService.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Diagnostics/DiagnosticsService.cpp', 'Module'] = MODULE_FW_MNGT
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/FirmwareManagement/FirmwareService.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/FirmwareManagement/FirmwareService.cpp', 'Module'] = MODULE_FW_MNGT
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Heartbeat/HeartbeatService.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Heartbeat/HeartbeatService.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Heartbeat/HeartbeatService.cpp', 'Module'] = MODULE_AVAILABILITY
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Metering/MeteringConnector.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Metering/MeteringConnector.cpp', 'Module'] = MODULE_METERVALUES
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Metering/MeteringService.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Metering/MeteringService.cpp', 'Module'] = MODULE_METERVALUES
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Metering/MeterStore.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Metering/MeterStore.cpp', 'Module'] = MODULE_METERVALUES
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Metering/MeterValue.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Metering/MeterValue.cpp', 'Module'] = MODULE_METERVALUES
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Metering/SampledValue.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Metering/SampledValue.cpp', 'Module'] = MODULE_METERVALUES
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Model.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Model.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Model.cpp', 'Module'] = MODULE_GENERAL
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Reservation/Reservation.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Reservation/Reservation.cpp', 'Module'] = MODULE_RESERVATION
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Reservation/ReservationService.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Reservation/ReservationService.cpp', 'Module'] = MODULE_RESERVATION
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Reset/ResetService.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Reset/ResetService.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Reset/ResetService.cpp', 'Module'] = MODULE_PROVISIONING
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/SmartCharging/SmartChargingModel.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/SmartCharging/SmartChargingModel.cpp', 'Module'] = MODULE_SMARTCHARGING
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/SmartCharging/SmartChargingService.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/SmartCharging/SmartChargingService.cpp', 'Module'] = MODULE_SMARTCHARGING
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Transactions/Transaction.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Transactions/Transaction.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Transactions/Transaction.cpp', 'Module'] = MODULE_TX
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Transactions/TransactionDeserialize.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Transactions/TransactionDeserialize.cpp', 'Module'] = MODULE_TX
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Transactions/TransactionService.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Transactions/TransactionService.cpp', 'Module'] = MODULE_TX
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Transactions/TransactionStore.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Transactions/TransactionStore.cpp', 'Module'] = MODULE_TX
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Variables/Variable.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Variables/Variable.cpp', 'Module'] = MODULE_PROVISIONING_VARS
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Variables/VariableContainer.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Variables/VariableContainer.cpp', 'Module'] = MODULE_PROVISIONING_VARS
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Variables/VariableService.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Model/Variables/VariableService.cpp', 'Module'] = MODULE_PROVISIONING_VARS
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/Authorize.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/Authorize.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/Authorize.cpp', 'Module'] = MODULE_AUTHORIZATION
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/BootNotification.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/BootNotification.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/BootNotification.cpp', 'Module'] = MODULE_PROVISIONING
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/CancelReservation.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/CancelReservation.cpp', 'Module'] = MODULE_RESERVATION
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/ChangeAvailability.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/ChangeAvailability.cpp', 'Module'] = MODULE_AVAILABILITY
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/ChangeConfiguration.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/ChangeConfiguration.cpp', 'Module'] = MODULE_CONFIGURATION
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/ClearCache.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/ClearCache.cpp', 'Module'] = MODULE_CORE
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/ClearChargingProfile.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/ClearChargingProfile.cpp', 'Module'] = MODULE_SMARTCHARGING
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/CustomOperation.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/CustomOperation.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/CustomOperation.cpp', 'Module'] = MODULE_RPC
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/DataTransfer.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/DataTransfer.cpp', 'Module'] = MODULE_CORE
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/DeleteCertificate.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/DeleteCertificate.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/DeleteCertificate.cpp', 'Module'] = MODULE_CERTS
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/DiagnosticsStatusNotification.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/DiagnosticsStatusNotification.cpp', 'Module'] = MODULE_FW_MNGT
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/FirmwareStatusNotification.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/FirmwareStatusNotification.cpp', 'Module'] = MODULE_FW_MNGT
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/GetBaseReport.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/GetBaseReport.cpp', 'Module'] = MODULE_PROVISIONING_VARS
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/GetCompositeSchedule.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/GetCompositeSchedule.cpp', 'Module'] = MODULE_SMARTCHARGING
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/GetConfiguration.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/GetConfiguration.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/GetConfiguration.cpp', 'Module'] = MODULE_CONFIGURATION
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/GetDiagnostics.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/GetDiagnostics.cpp', 'Module'] = MODULE_FW_MNGT
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/GetInstalledCertificateIds.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/GetInstalledCertificateIds.cpp', 'Module'] = MODULE_SMARTCHARGING
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/GetLocalListVersion.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/GetLocalListVersion.cpp', 'Module'] = MODULE_LOCALAUTH
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/GetVariables.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/GetVariables.cpp', 'Module'] = MODULE_PROVISIONING_VARS
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/Heartbeat.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/Heartbeat.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/Heartbeat.cpp', 'Module'] = MODULE_AVAILABILITY
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/InstallCertificate.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/InstallCertificate.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/InstallCertificate.cpp', 'Module'] = MODULE_CERTS
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/MeterValues.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/MeterValues.cpp', 'Module'] = MODULE_METERVALUES
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/NotifyReport.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/NotifyReport.cpp', 'Module'] = MODULE_PROVISIONING_VARS
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/RemoteStartTransaction.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/RemoteStartTransaction.cpp', 'Module'] = MODULE_TX
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/RemoteStopTransaction.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/RemoteStopTransaction.cpp', 'Module'] = MODULE_TX
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/RequestStartTransaction.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/RequestStartTransaction.cpp', 'Module'] = MODULE_TX
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/RequestStopTransaction.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/RequestStopTransaction.cpp', 'Module'] = MODULE_TX
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/ReserveNow.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/ReserveNow.cpp', 'Module'] = MODULE_RESERVATION
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/Reset.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/Reset.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/Reset.cpp', 'Module'] = MODULE_PROVISIONING
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/SecurityEventNotification.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/SecurityEventNotification.cpp', 'Module'] = MODULE_SECURITY
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/SendLocalList.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/SendLocalList.cpp', 'Module'] = MODULE_LOCALAUTH
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/SetChargingProfile.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/SetChargingProfile.cpp', 'Module'] = MODULE_SMARTCHARGING
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/SetVariables.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/SetVariables.cpp', 'Module'] = MODULE_PROVISIONING_VARS
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/StartTransaction.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/StartTransaction.cpp', 'Module'] = MODULE_TX
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/StatusNotification.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/StatusNotification.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/StatusNotification.cpp', 'Module'] = MODULE_AVAILABILITY
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/StopTransaction.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/StopTransaction.cpp', 'Module'] = MODULE_TX
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/TransactionEvent.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/TransactionEvent.cpp', 'Module'] = MODULE_TX
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/TriggerMessage.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/TriggerMessage.cpp', 'Module'] = MODULE_TRIGGERMESSAGE
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/UnlockConnector.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/UnlockConnector.cpp', 'Module'] = MODULE_CORE
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/UpdateFirmware.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp/Operations/UpdateFirmware.cpp', 'Module'] = MODULE_FW_MNGT
    df.at['lib/MicroOcpp/src/MicroOcpp_c.cpp', 'v16'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp_c.cpp', 'v201'] = TICK
    df.at['lib/MicroOcpp/src/MicroOcpp_c.cpp', 'Module'] = MODULE_GENERAL

    print(df)

categorize_table(cunits_v16)
categorize_table(cunits_v201)

# store csv with all details

print('Uncategorized compile units (v16): ', (cunits_v16['Module'].values == '').sum())
print('Uncategorized compile units (v201): ', (cunits_v201['Module'].values == '').sum())

cunits_v16.to_csv("docs/assets/tables/compile_units_v16.csv")
cunits_v16.to_csv("docs/assets/tables/compile_units_v201.csv")

# store csv with size by Module for v16

modules_v16 = cunits_v16.loc[cunits_v16['v16'].values == 'x'].sort_index()
modules_v16_by_module = modules_v16[['Module', COLUMN_BINSIZE]].groupby('Module').sum()
modules_v16_by_module.loc['**Total**'] = [modules_v16_by_module[COLUMN_BINSIZE].sum()]

print(modules_v16_by_module)

modules_v16_by_module.to_csv('docs/assets/tables/modules_v16.csv')

# store csv with size by Module for v201

modules_v201 = cunits_v201.loc[cunits_v201['v201'].values == 'x'].sort_index()
modules_v201_by_module = modules_v201[['Module', COLUMN_BINSIZE]].groupby('Module').sum()
modules_v201_by_module.loc['**Total**'] = [modules_v201_by_module[COLUMN_BINSIZE].sum()]

print(modules_v201_by_module)

modules_v201_by_module.to_csv('docs/assets/tables/modules_v201.csv')
