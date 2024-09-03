import numpy as np
import pandas as pd

# load data

COLUMN_BINSIZE = 'Binary size (Bytes)'

def load_compilation_units(fn):
    df = pd.read_csv(fn, index_col="compileunits").filter(like="lib/MicroOcpp/src/MicroOcpp", axis=0).filter(['Module','v16','v201','vmsize'], axis=1).sort_index()
    df.index.names = ['Compile Unit']
    df.index = df.index.map(lambda s: s[len("lib/MicroOcpp/src/"):] if s.startswith("lib/MicroOcpp/src/") else s)
    df.index = df.index.map(lambda s: s[len("MicroOcpp/"):] if s.startswith("MicroOcpp/") else s)
    df.rename(columns={'vmsize': COLUMN_BINSIZE}, inplace=True)
    return df
    
cunits_v16 = load_compilation_units('docs/assets/tables/bloaty_v16.csv')
cunits_v201 = load_compilation_units('docs/assets/tables/bloaty_v201.csv')

# categorize data

def categorize_table(df):

    df["v16"] = ' '
    df["v201"] = ' '
    df["Module"] = ' '

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

    df.at['MicroOcpp.cpp', 'v16'] = TICK
    df.at['MicroOcpp.cpp', 'v201'] = TICK
    df.at['MicroOcpp.cpp', 'Module'] = MODULE_GENERAL
    df.at['Core/Configuration.cpp', 'v16'] = TICK
    df.at['Core/Configuration.cpp', 'v201'] = TICK
    df.at['Core/Configuration.cpp', 'Module'] = MODULE_CONFIGURATION
    if 'Core/Configuration_c.cpp' in df.index:
        df.at['Core/Configuration_c.cpp', 'v16'] = TICK
        df.at['Core/Configuration_c.cpp', 'v201'] = TICK
        df.at['Core/Configuration_c.cpp', 'Module'] = MODULE_CONFIGURATION
    df.at['Core/ConfigurationContainer.cpp', 'v16'] = TICK
    df.at['Core/ConfigurationContainer.cpp', 'v201'] = TICK
    df.at['Core/ConfigurationContainer.cpp', 'Module'] = MODULE_CONFIGURATION
    df.at['Core/ConfigurationContainerFlash.cpp', 'v16'] = TICK
    df.at['Core/ConfigurationContainerFlash.cpp', 'v201'] = TICK
    df.at['Core/ConfigurationContainerFlash.cpp', 'Module'] = MODULE_CONFIGURATION
    df.at['Core/ConfigurationKeyValue.cpp', 'v16'] = TICK
    df.at['Core/ConfigurationKeyValue.cpp', 'v201'] = TICK
    df.at['Core/ConfigurationKeyValue.cpp', 'Module'] = MODULE_CONFIGURATION
    df.at['Core/Connection.cpp', 'v16'] = TICK
    df.at['Core/Connection.cpp', 'v201'] = TICK
    df.at['Core/Connection.cpp', 'Module'] = MODULE_HAL
    df.at['Core/Context.cpp', 'v16'] = TICK
    df.at['Core/Context.cpp', 'v201'] = TICK
    df.at['Core/Context.cpp', 'Module'] = MODULE_GENERAL
    df.at['Core/FilesystemAdapter.cpp', 'v16'] = TICK
    df.at['Core/FilesystemAdapter.cpp', 'v201'] = TICK
    df.at['Core/FilesystemAdapter.cpp', 'Module'] = MODULE_HAL
    df.at['Core/FilesystemUtils.cpp', 'v16'] = TICK
    df.at['Core/FilesystemUtils.cpp', 'v201'] = TICK
    df.at['Core/FilesystemUtils.cpp', 'Module'] = MODULE_GENERAL
    df.at['Core/FtpMbedTLS.cpp', 'v16'] = TICK
    df.at['Core/FtpMbedTLS.cpp', 'v201'] = TICK
    df.at['Core/FtpMbedTLS.cpp', 'Module'] = MODULE_GENERAL
    df.at['Core/Memory.cpp', 'v16'] = TICK
    df.at['Core/Memory.cpp', 'v201'] = TICK
    df.at['Core/Memory.cpp', 'Module'] = MODULE_TESTS
    df.at['Core/Operation.cpp', 'v16'] = TICK
    df.at['Core/Operation.cpp', 'v201'] = TICK
    df.at['Core/Operation.cpp', 'Module'] = MODULE_RPC
    df.at['Core/OperationRegistry.cpp', 'v16'] = TICK
    df.at['Core/OperationRegistry.cpp', 'v201'] = TICK
    df.at['Core/OperationRegistry.cpp', 'Module'] = MODULE_RPC
    df.at['Core/Request.cpp', 'v16'] = TICK
    df.at['Core/Request.cpp', 'v201'] = TICK
    df.at['Core/Request.cpp', 'Module'] = MODULE_RPC
    df.at['Core/RequestQueue.cpp', 'v16'] = TICK
    df.at['Core/RequestQueue.cpp', 'v201'] = TICK
    df.at['Core/RequestQueue.cpp', 'Module'] = MODULE_RPC
    df.at['Core/Time.cpp', 'v16'] = TICK
    df.at['Core/Time.cpp', 'v201'] = TICK
    df.at['Core/Time.cpp', 'Module'] = MODULE_GENERAL
    if 'Debug.cpp' in df.index:
        df.at['Debug.cpp', 'v16'] = TICK
        df.at['Debug.cpp', 'v201'] = TICK
        df.at['Debug.cpp', 'Module'] = MODULE_HAL
    df.at['Model/Authorization/AuthorizationData.cpp', 'v16'] = TICK
    df.at['Model/Authorization/AuthorizationData.cpp', 'Module'] = MODULE_LOCALAUTH
    df.at['Model/Authorization/AuthorizationList.cpp', 'v16'] = TICK
    df.at['Model/Authorization/AuthorizationList.cpp', 'Module'] = MODULE_LOCALAUTH
    df.at['Model/Authorization/AuthorizationService.cpp', 'v16'] = TICK
    df.at['Model/Authorization/AuthorizationService.cpp', 'Module'] = MODULE_LOCALAUTH
    if 'Model/Authorization/IdToken.cpp' in df.index:
        df.at['Model/Authorization/IdToken.cpp', 'v201'] = TICK
        df.at['Model/Authorization/IdToken.cpp', 'Module'] = MODULE_AUTHORIZATION
    if 'Model/Availability/AvailabilityService.cpp' in df.index:
        df.at['Model/Availability/AvailabilityService.cpp', 'v16'] = TICK
        df.at['Model/Availability/AvailabilityService.cpp', 'v201'] = TICK
        df.at['Model/Availability/AvailabilityService.cpp', 'Module'] = MODULE_AVAILABILITY
    df.at['Model/Boot/BootService.cpp', 'v16'] = TICK
    df.at['Model/Boot/BootService.cpp', 'v201'] = TICK
    df.at['Model/Boot/BootService.cpp', 'Module'] = MODULE_PROVISIONING
    df.at['Model/Certificates/Certificate.cpp', 'v16'] = TICK
    df.at['Model/Certificates/Certificate.cpp', 'v201'] = TICK
    df.at['Model/Certificates/Certificate.cpp', 'Module'] = MODULE_CERTS
    df.at['Model/Certificates/CertificateMbedTLS.cpp', 'v16'] = TICK
    df.at['Model/Certificates/CertificateMbedTLS.cpp', 'v201'] = TICK
    df.at['Model/Certificates/CertificateMbedTLS.cpp', 'Module'] = MODULE_CERTS
    df.at['Model/Certificates/Certificate_c.cpp', 'v16'] = TICK
    df.at['Model/Certificates/Certificate_c.cpp', 'v201'] = TICK
    df.at['Model/Certificates/Certificate_c.cpp', 'Module'] = MODULE_CERTS
    df.at['Model/Certificates/CertificateService.cpp', 'v16'] = TICK
    df.at['Model/Certificates/CertificateService.cpp', 'v201'] = TICK
    df.at['Model/Certificates/CertificateService.cpp', 'Module'] = MODULE_CERTS
    df.at['Model/ConnectorBase/Connector.cpp', 'v16'] = TICK
    df.at['Model/ConnectorBase/Connector.cpp', 'Module'] = MODULE_CORE
    df.at['Model/ConnectorBase/ConnectorsCommon.cpp', 'v16'] = TICK
    df.at['Model/ConnectorBase/ConnectorsCommon.cpp', 'Module'] = MODULE_CORE
    df.at['Model/Diagnostics/DiagnosticsService.cpp', 'v16'] = TICK
    df.at['Model/Diagnostics/DiagnosticsService.cpp', 'Module'] = MODULE_FW_MNGT
    df.at['Model/FirmwareManagement/FirmwareService.cpp', 'v16'] = TICK
    df.at['Model/FirmwareManagement/FirmwareService.cpp', 'Module'] = MODULE_FW_MNGT
    df.at['Model/Heartbeat/HeartbeatService.cpp', 'v16'] = TICK
    df.at['Model/Heartbeat/HeartbeatService.cpp', 'v201'] = TICK
    df.at['Model/Heartbeat/HeartbeatService.cpp', 'Module'] = MODULE_AVAILABILITY
    df.at['Model/Metering/MeteringConnector.cpp', 'v16'] = TICK
    df.at['Model/Metering/MeteringConnector.cpp', 'Module'] = MODULE_METERVALUES
    df.at['Model/Metering/MeteringService.cpp', 'v16'] = TICK
    df.at['Model/Metering/MeteringService.cpp', 'Module'] = MODULE_METERVALUES
    df.at['Model/Metering/MeterStore.cpp', 'v16'] = TICK
    df.at['Model/Metering/MeterStore.cpp', 'Module'] = MODULE_METERVALUES
    df.at['Model/Metering/MeterValue.cpp', 'v16'] = TICK
    df.at['Model/Metering/MeterValue.cpp', 'Module'] = MODULE_METERVALUES
    df.at['Model/Metering/SampledValue.cpp', 'v16'] = TICK
    df.at['Model/Metering/SampledValue.cpp', 'Module'] = MODULE_METERVALUES
    df.at['Model/Model.cpp', 'v16'] = TICK
    df.at['Model/Model.cpp', 'v201'] = TICK
    df.at['Model/Model.cpp', 'Module'] = MODULE_GENERAL
    df.at['Model/Reservation/Reservation.cpp', 'v16'] = TICK
    df.at['Model/Reservation/Reservation.cpp', 'Module'] = MODULE_RESERVATION
    df.at['Model/Reservation/ReservationService.cpp', 'v16'] = TICK
    df.at['Model/Reservation/ReservationService.cpp', 'Module'] = MODULE_RESERVATION
    df.at['Model/Reset/ResetService.cpp', 'v16'] = TICK
    df.at['Model/Reset/ResetService.cpp', 'v201'] = TICK
    df.at['Model/Reset/ResetService.cpp', 'Module'] = MODULE_PROVISIONING
    df.at['Model/SmartCharging/SmartChargingModel.cpp', 'v16'] = TICK
    df.at['Model/SmartCharging/SmartChargingModel.cpp', 'Module'] = MODULE_SMARTCHARGING
    df.at['Model/SmartCharging/SmartChargingService.cpp', 'v16'] = TICK
    df.at['Model/SmartCharging/SmartChargingService.cpp', 'Module'] = MODULE_SMARTCHARGING
    df.at['Model/Transactions/Transaction.cpp', 'v16'] = TICK
    df.at['Model/Transactions/Transaction.cpp', 'v201'] = TICK
    df.at['Model/Transactions/Transaction.cpp', 'Module'] = MODULE_TX
    df.at['Model/Transactions/TransactionDeserialize.cpp', 'v16'] = TICK
    df.at['Model/Transactions/TransactionDeserialize.cpp', 'Module'] = MODULE_TX
    df.at['Model/Transactions/TransactionService.cpp', 'v201'] = TICK
    df.at['Model/Transactions/TransactionService.cpp', 'Module'] = MODULE_TX
    df.at['Model/Transactions/TransactionStore.cpp', 'v16'] = TICK
    df.at['Model/Transactions/TransactionStore.cpp', 'Module'] = MODULE_TX
    df.at['Model/Variables/Variable.cpp', 'v201'] = TICK
    df.at['Model/Variables/Variable.cpp', 'Module'] = MODULE_PROVISIONING_VARS
    df.at['Model/Variables/VariableContainer.cpp', 'v201'] = TICK
    df.at['Model/Variables/VariableContainer.cpp', 'Module'] = MODULE_PROVISIONING_VARS
    df.at['Model/Variables/VariableService.cpp', 'v201'] = TICK
    df.at['Model/Variables/VariableService.cpp', 'Module'] = MODULE_PROVISIONING_VARS
    df.at['Operations/Authorize.cpp', 'v16'] = TICK
    df.at['Operations/Authorize.cpp', 'v201'] = TICK
    df.at['Operations/Authorize.cpp', 'Module'] = MODULE_AUTHORIZATION
    df.at['Operations/BootNotification.cpp', 'v16'] = TICK
    df.at['Operations/BootNotification.cpp', 'v201'] = TICK
    df.at['Operations/BootNotification.cpp', 'Module'] = MODULE_PROVISIONING
    df.at['Operations/CancelReservation.cpp', 'v16'] = TICK
    df.at['Operations/CancelReservation.cpp', 'Module'] = MODULE_RESERVATION
    df.at['Operations/ChangeAvailability.cpp', 'v16'] = TICK
    df.at['Operations/ChangeAvailability.cpp', 'Module'] = MODULE_AVAILABILITY
    df.at['Operations/ChangeConfiguration.cpp', 'v16'] = TICK
    df.at['Operations/ChangeConfiguration.cpp', 'Module'] = MODULE_CONFIGURATION
    df.at['Operations/ClearCache.cpp', 'v16'] = TICK
    df.at['Operations/ClearCache.cpp', 'Module'] = MODULE_CORE
    df.at['Operations/ClearChargingProfile.cpp', 'v16'] = TICK
    df.at['Operations/ClearChargingProfile.cpp', 'Module'] = MODULE_SMARTCHARGING
    df.at['Operations/CustomOperation.cpp', 'v16'] = TICK
    df.at['Operations/CustomOperation.cpp', 'v201'] = TICK
    df.at['Operations/CustomOperation.cpp', 'Module'] = MODULE_RPC
    df.at['Operations/DataTransfer.cpp', 'v16'] = TICK
    df.at['Operations/DataTransfer.cpp', 'Module'] = MODULE_CORE
    df.at['Operations/DeleteCertificate.cpp', 'v16'] = TICK
    df.at['Operations/DeleteCertificate.cpp', 'v201'] = TICK
    df.at['Operations/DeleteCertificate.cpp', 'Module'] = MODULE_CERTS
    df.at['Operations/DiagnosticsStatusNotification.cpp', 'v16'] = TICK
    df.at['Operations/DiagnosticsStatusNotification.cpp', 'Module'] = MODULE_FW_MNGT
    df.at['Operations/FirmwareStatusNotification.cpp', 'v16'] = TICK
    df.at['Operations/FirmwareStatusNotification.cpp', 'Module'] = MODULE_FW_MNGT
    df.at['Operations/GetBaseReport.cpp', 'v201'] = TICK
    df.at['Operations/GetBaseReport.cpp', 'Module'] = MODULE_PROVISIONING_VARS
    df.at['Operations/GetCompositeSchedule.cpp', 'v16'] = TICK
    df.at['Operations/GetCompositeSchedule.cpp', 'Module'] = MODULE_SMARTCHARGING
    df.at['Operations/GetConfiguration.cpp', 'v16'] = TICK
    df.at['Operations/GetConfiguration.cpp', 'v201'] = TICK
    df.at['Operations/GetConfiguration.cpp', 'Module'] = MODULE_CONFIGURATION
    df.at['Operations/GetDiagnostics.cpp', 'v16'] = TICK
    df.at['Operations/GetDiagnostics.cpp', 'Module'] = MODULE_FW_MNGT
    df.at['Operations/GetInstalledCertificateIds.cpp', 'v16'] = TICK
    df.at['Operations/GetInstalledCertificateIds.cpp', 'Module'] = MODULE_SMARTCHARGING
    df.at['Operations/GetLocalListVersion.cpp', 'v16'] = TICK
    df.at['Operations/GetLocalListVersion.cpp', 'Module'] = MODULE_LOCALAUTH
    df.at['Operations/GetVariables.cpp', 'v201'] = TICK
    df.at['Operations/GetVariables.cpp', 'Module'] = MODULE_PROVISIONING_VARS
    df.at['Operations/Heartbeat.cpp', 'v16'] = TICK
    df.at['Operations/Heartbeat.cpp', 'v201'] = TICK
    df.at['Operations/Heartbeat.cpp', 'Module'] = MODULE_AVAILABILITY
    df.at['Operations/InstallCertificate.cpp', 'v16'] = TICK
    df.at['Operations/InstallCertificate.cpp', 'v201'] = TICK
    df.at['Operations/InstallCertificate.cpp', 'Module'] = MODULE_CERTS
    df.at['Operations/MeterValues.cpp', 'v16'] = TICK
    df.at['Operations/MeterValues.cpp', 'Module'] = MODULE_METERVALUES
    df.at['Operations/NotifyReport.cpp', 'v201'] = TICK
    df.at['Operations/NotifyReport.cpp', 'Module'] = MODULE_PROVISIONING_VARS
    df.at['Operations/RemoteStartTransaction.cpp', 'v16'] = TICK
    df.at['Operations/RemoteStartTransaction.cpp', 'Module'] = MODULE_TX
    df.at['Operations/RemoteStopTransaction.cpp', 'v16'] = TICK
    df.at['Operations/RemoteStopTransaction.cpp', 'Module'] = MODULE_TX
    df.at['Operations/RequestStartTransaction.cpp', 'v201'] = TICK
    df.at['Operations/RequestStartTransaction.cpp', 'Module'] = MODULE_TX
    df.at['Operations/RequestStopTransaction.cpp', 'v201'] = TICK
    df.at['Operations/RequestStopTransaction.cpp', 'Module'] = MODULE_TX
    df.at['Operations/ReserveNow.cpp', 'v16'] = TICK
    df.at['Operations/ReserveNow.cpp', 'Module'] = MODULE_RESERVATION
    df.at['Operations/Reset.cpp', 'v16'] = TICK
    df.at['Operations/Reset.cpp', 'v201'] = TICK
    df.at['Operations/Reset.cpp', 'Module'] = MODULE_PROVISIONING
    df.at['Operations/SecurityEventNotification.cpp', 'v201'] = TICK
    df.at['Operations/SecurityEventNotification.cpp', 'Module'] = MODULE_SECURITY
    df.at['Operations/SendLocalList.cpp', 'v16'] = TICK
    df.at['Operations/SendLocalList.cpp', 'Module'] = MODULE_LOCALAUTH
    df.at['Operations/SetChargingProfile.cpp', 'v16'] = TICK
    df.at['Operations/SetChargingProfile.cpp', 'Module'] = MODULE_SMARTCHARGING
    df.at['Operations/SetVariables.cpp', 'v201'] = TICK
    df.at['Operations/SetVariables.cpp', 'Module'] = MODULE_PROVISIONING_VARS
    df.at['Operations/StartTransaction.cpp', 'v16'] = TICK
    df.at['Operations/StartTransaction.cpp', 'Module'] = MODULE_TX
    df.at['Operations/StatusNotification.cpp', 'v16'] = TICK
    df.at['Operations/StatusNotification.cpp', 'v201'] = TICK
    df.at['Operations/StatusNotification.cpp', 'Module'] = MODULE_AVAILABILITY
    df.at['Operations/StopTransaction.cpp', 'v16'] = TICK
    df.at['Operations/StopTransaction.cpp', 'Module'] = MODULE_TX
    df.at['Operations/TransactionEvent.cpp', 'v201'] = TICK
    df.at['Operations/TransactionEvent.cpp', 'Module'] = MODULE_TX
    df.at['Operations/TriggerMessage.cpp', 'v16'] = TICK
    df.at['Operations/TriggerMessage.cpp', 'Module'] = MODULE_TRIGGERMESSAGE
    df.at['Operations/UnlockConnector.cpp', 'v16'] = TICK
    df.at['Operations/UnlockConnector.cpp', 'Module'] = MODULE_CORE
    df.at['Operations/UpdateFirmware.cpp', 'v16'] = TICK
    df.at['Operations/UpdateFirmware.cpp', 'Module'] = MODULE_FW_MNGT
    df.at['MicroOcpp_c.cpp', 'v16'] = TICK
    df.at['MicroOcpp_c.cpp', 'v201'] = TICK
    df.at['MicroOcpp_c.cpp', 'Module'] = MODULE_GENERAL

    print(df)

categorize_table(cunits_v16)
categorize_table(cunits_v201)

# store csv with all details

print('Uncategorized compile units (v16): ', (cunits_v16['Module'].values == '').sum())
print('Uncategorized compile units (v201): ', (cunits_v201['Module'].values == '').sum())

cunits_v16.to_csv("docs/assets/tables/compile_units_v16.csv")
cunits_v201.to_csv("docs/assets/tables/compile_units_v201.csv")

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
