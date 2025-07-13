import sys
import numpy as np
import pandas as pd

# load data

COLUMN_CUNITS = 'Compile Unit'
COLUMN_SIZE_V16 = 'OCPP 1.6 (Bytes)'
COLUMN_SIZE_V201 = 'OCPP 2.0.1 (Bytes)'
COLUMN_SIZE_V16_V201 = 'Both enabled (Bytes)'

def load_compilation_units(fn):
    df = pd.read_csv(fn, index_col="compileunits").filter(like="src/MicroOcpp", axis=0).filter(['Module','vmsize'], axis=1).sort_index()
    df.index.names = [COLUMN_CUNITS]
    df.index = df.index.map(lambda s: s[len("lib/MicroOcpp/src/"):] if s.startswith("lib/MicroOcpp/src/") else s)
    df.index = df.index.map(lambda s: s[len("src/MicroOcpp/"):] if s.startswith("src/MicroOcpp/") else s)
    df.index = df.index.map(lambda s: s[len("src/"):] if s.startswith("src/") else s)
    df.index = df.index.map(lambda s: s[len("MicroOcpp/"):] if s.startswith("MicroOcpp/") else s)
    return df
    
cunits_v16 = load_compilation_units('docs/assets/tables/bloaty_v16.csv')
cunits_v16.rename(columns={'vmsize': COLUMN_SIZE_V16}, inplace=True)
cunits_v201 = load_compilation_units('docs/assets/tables/bloaty_v201.csv')
cunits_v201.rename(columns={'vmsize': COLUMN_SIZE_V201}, inplace=True)
cunits_v16_v201 = load_compilation_units('docs/assets/tables/bloaty_v16_v201.csv')
cunits_v16_v201.rename(columns={'vmsize': COLUMN_SIZE_V16_V201}, inplace=True)

cunits = pd.merge(cunits_v16, cunits_v201, on=COLUMN_CUNITS, how='outer')
cunits = pd.merge(cunits, cunits_v16_v201, on=COLUMN_CUNITS, how='outer')

print("cunits_v16")
print(cunits_v16)
print("cunits_v201")
print(cunits_v201)
print("cunits_v16_v201")
print(cunits_v16_v201)
print("cunits")
print(cunits)

# categorize data

def categorize_table(df):

    df["Module"] = ''

    MODULE_MO = ' General library functions'
    MODULE_SECURITY = 'A - Security'
    MODULE_PROVISIONING_CONF = 'B - Provisioning -  Configuration (v16)'
    MODULE_PROVISIONING_VARS = 'B - Provisioning -  Variables (v201)'
    MODULE_PROVISIONING = 'B - Provisioning - Other'
    MODULE_AUTHORIZATION = 'C - Authorization'
    MODULE_LOCALAUTH = 'D - LocalAuthorizationListManagement'
    MODULE_TX = 'E - Transactions'
    MODULE_REMOTECONTROL = 'F - RemoteControl'
    MODULE_AVAILABILITY = 'G - Availability'
    MODULE_RESERVATION = 'H - Reservation'
    MODULE_METERVALUES = 'J - MeterValues'
    MODULE_SMARTCHARGING = 'K - SmartCharging'
    MODULE_FW_MNGT = 'L - FirmwareManagement'
    MODULE_CERTS = 'M - CertificateManagement'
    MODULE_DIAG = 'N - Diagnostics'
    MODULE_DATATRANSFER = 'P - DataTransfer'

    df.at['Context.cpp', 'Module'] = MODULE_MO
    df.at['Core/Connection.cpp', 'Module'] = MODULE_MO
    df.at['Core/FilesystemAdapter.cpp', 'Module'] = MODULE_MO
    df.at['Core/FilesystemUtils.cpp', 'Module'] = MODULE_MO
    df.at['Core/FtpMbedTLS.cpp', 'Module'] = MODULE_MO
    df.at['Core/Memory.cpp', 'Module'] = MODULE_MO
    df.at['Core/MessageService.cpp', 'Module'] = MODULE_MO
    df.at['Core/Operation.cpp', 'Module'] = MODULE_MO
    df.at['Core/PersistencyUtils.cpp', 'Module'] = MODULE_MO
    df.at['Core/Request.cpp', 'Module'] = MODULE_MO
    df.at['Core/RequestQueue.cpp', 'Module'] = MODULE_MO
    df.at['Core/Time.cpp', 'Module'] = MODULE_MO
    df.at['Core/UuidUtils.cpp', 'Module'] = MODULE_MO
    df.at['Debug.cpp', 'Module'] = MODULE_MO
    df.at['MicroOcpp.cpp', 'Module'] = MODULE_MO
    df.at['Model/Authorization/AuthorizationData.cpp', 'Module'] = MODULE_LOCALAUTH
    df.at['Model/Authorization/AuthorizationList.cpp', 'Module'] = MODULE_LOCALAUTH
    df.at['Model/Authorization/AuthorizationService.cpp', 'Module'] = MODULE_LOCALAUTH
    df.at['Model/Authorization/IdToken.cpp', 'Module'] = MODULE_AUTHORIZATION
    df.at['Model/Availability/AvailabilityDefs.cpp', 'Module'] = MODULE_AVAILABILITY
    df.at['Model/Availability/AvailabilityService.cpp', 'Module'] = MODULE_AVAILABILITY
    df.at['Model/Boot/BootService.cpp', 'Module'] = MODULE_PROVISIONING
    df.at['Model/Certificates/Certificate.cpp', 'Module'] = MODULE_CERTS
    df.at['Model/Certificates/CertificateMbedTLS.cpp', 'Module'] = MODULE_CERTS
    df.at['Model/Certificates/CertificateService.cpp', 'Module'] = MODULE_CERTS
    df.at['Model/Certificates/Certificate_c.cpp', 'Module'] = MODULE_CERTS
    df.at['Model/Configuration/Configuration.cpp', 'Module'] = MODULE_PROVISIONING_CONF
    df.at['Model/Configuration/ConfigurationContainer.cpp', 'Module'] = MODULE_PROVISIONING_CONF
    df.at['Model/Configuration/ConfigurationService.cpp', 'Module'] = MODULE_PROVISIONING_CONF
    df.at['Model/Diagnostics/Diagnostics.cpp', 'Module'] = MODULE_DIAG
    df.at['Model/Diagnostics/DiagnosticsService.cpp', 'Module'] = MODULE_DIAG
    df.at['Model/FirmwareManagement/FirmwareService.cpp', 'Module'] = MODULE_FW_MNGT
    df.at['Model/Heartbeat/HeartbeatService.cpp', 'Module'] = MODULE_AVAILABILITY
    df.at['Model/Metering/MeteringService.cpp', 'Module'] = MODULE_METERVALUES
    df.at['Model/Metering/MeterStore.cpp', 'Module'] = MODULE_METERVALUES
    df.at['Model/Metering/MeterValue.cpp', 'Module'] = MODULE_METERVALUES
    df.at['Model/Metering/ReadingContext.cpp', 'Module'] = MODULE_METERVALUES
    df.at['Model/Model.cpp', 'Module'] = MODULE_MO
    df.at['Model/RemoteControl/RemoteControlService.cpp', 'Module'] = MODULE_REMOTECONTROL
    df.at['Model/Reservation/Reservation.cpp', 'Module'] = MODULE_RESERVATION
    df.at['Model/Reservation/ReservationService.cpp', 'Module'] = MODULE_RESERVATION
    df.at['Model/Reset/ResetService.cpp', 'Module'] = MODULE_PROVISIONING
    df.at['Model/SecurityEvent/SecurityEventService.cpp', 'Module'] = MODULE_SECURITY
    df.at['Model/SmartCharging/SmartChargingModel.cpp', 'Module'] = MODULE_SMARTCHARGING
    df.at['Model/SmartCharging/SmartChargingService.cpp', 'Module'] = MODULE_SMARTCHARGING
    df.at['Model/Transactions/Transaction.cpp', 'Module'] = MODULE_TX
    df.at['Model/Transactions/TransactionService16.cpp', 'Module'] = MODULE_TX
    df.at['Model/Transactions/TransactionService201.cpp', 'Module'] = MODULE_TX
    df.at['Model/Transactions/TransactionStore.cpp', 'Module'] = MODULE_TX
    df.at['Model/Variables/Variable.cpp', 'Module'] = MODULE_PROVISIONING_VARS
    df.at['Model/Variables/VariableContainer.cpp', 'Module'] = MODULE_PROVISIONING_VARS
    df.at['Model/Variables/VariableService.cpp', 'Module'] = MODULE_PROVISIONING_VARS
    df.at['Operations/Authorize.cpp', 'Module'] = MODULE_AUTHORIZATION
    df.at['Operations/BootNotification.cpp', 'Module'] = MODULE_PROVISIONING
    df.at['Operations/CancelReservation.cpp', 'Module'] = MODULE_RESERVATION
    df.at['Operations/ChangeAvailability.cpp', 'Module'] = MODULE_AVAILABILITY
    df.at['Operations/ChangeConfiguration.cpp', 'Module'] = MODULE_PROVISIONING_CONF
    df.at['Operations/ClearCache.cpp', 'Module'] = MODULE_AUTHORIZATION
    df.at['Operations/ClearChargingProfile.cpp', 'Module'] = MODULE_SMARTCHARGING
    df.at['Operations/CustomOperation.cpp', 'Module'] = MODULE_MO
    df.at['Operations/DataTransfer.cpp', 'Module'] = MODULE_DATATRANSFER
    df.at['Operations/DeleteCertificate.cpp', 'Module'] = MODULE_CERTS
    df.at['Operations/DiagnosticsStatusNotification.cpp', 'Module'] = MODULE_DIAG
    df.at['Operations/FirmwareStatusNotification.cpp', 'Module'] = MODULE_FW_MNGT
    df.at['Operations/GetBaseReport.cpp', 'Module'] = MODULE_PROVISIONING_VARS
    df.at['Operations/GetCompositeSchedule.cpp', 'Module'] = MODULE_SMARTCHARGING
    df.at['Operations/GetConfiguration.cpp', 'Module'] = MODULE_PROVISIONING_CONF
    df.at['Operations/GetDiagnostics.cpp', 'Module'] = MODULE_DIAG
    df.at['Operations/GetInstalledCertificateIds.cpp', 'Module'] = MODULE_CERTS
    df.at['Operations/GetLocalListVersion.cpp', 'Module'] = MODULE_LOCALAUTH
    df.at['Operations/GetLog.cpp', 'Module'] = MODULE_DIAG
    df.at['Operations/GetVariables.cpp', 'Module'] = MODULE_PROVISIONING_VARS
    df.at['Operations/Heartbeat.cpp', 'Module'] = MODULE_AVAILABILITY
    df.at['Operations/InstallCertificate.cpp', 'Module'] = MODULE_CERTS
    df.at['Operations/LogStatusNotification.cpp', 'Module'] = MODULE_DIAG
    df.at['Operations/MeterValues.cpp', 'Module'] = MODULE_METERVALUES
    df.at['Operations/NotifyReport.cpp', 'Module'] = MODULE_PROVISIONING_VARS
    df.at['Operations/RemoteStartTransaction.cpp', 'Module'] = MODULE_TX
    df.at['Operations/RemoteStopTransaction.cpp', 'Module'] = MODULE_TX
    df.at['Operations/RequestStartTransaction.cpp', 'Module'] = MODULE_TX
    df.at['Operations/RequestStopTransaction.cpp', 'Module'] = MODULE_TX
    df.at['Operations/ReserveNow.cpp', 'Module'] = MODULE_RESERVATION
    df.at['Operations/Reset.cpp', 'Module'] = MODULE_PROVISIONING
    df.at['Operations/SecurityEventNotification.cpp', 'Module'] = MODULE_SECURITY
    df.at['Operations/SendLocalList.cpp', 'Module'] = MODULE_LOCALAUTH
    df.at['Operations/SetChargingProfile.cpp', 'Module'] = MODULE_SMARTCHARGING
    df.at['Operations/SetVariables.cpp', 'Module'] = MODULE_PROVISIONING_VARS
    df.at['Operations/StartTransaction.cpp', 'Module'] = MODULE_TX
    df.at['Operations/StatusNotification.cpp', 'Module'] = MODULE_AVAILABILITY
    df.at['Operations/StopTransaction.cpp', 'Module'] = MODULE_TX
    df.at['Operations/TransactionEvent.cpp', 'Module'] = MODULE_TX
    df.at['Operations/TriggerMessage.cpp', 'Module'] = MODULE_REMOTECONTROL
    df.at['Operations/UnlockConnector.cpp', 'Module'] = MODULE_REMOTECONTROL
    df.at['Operations/UpdateFirmware.cpp', 'Module'] = MODULE_FW_MNGT
    df.at['Platform.cpp', 'Module'] = MODULE_MO

    print(df)

categorize_table(cunits)

categorize_success = True

if (cunits[[COLUMN_SIZE_V16, COLUMN_SIZE_V201, COLUMN_SIZE_V16_V201]].isna().all(axis=1)).any():
    print('Error: categorized non-existing compile units:\n')
    print(cunits.loc[cunits[[COLUMN_SIZE_V16, COLUMN_SIZE_V201, COLUMN_SIZE_V16_V201]].isna().all(axis=1)])
    categorize_success = False

if (cunits['Module'].values == '').sum() > 0:
    print('Error: did not categorize the following compilation units (v16):\n')
    print(cunits.loc[cunits['Module'].values == ''])
    categorize_success = False

if not categorize_success:
    sys.exit('\nError categorizing compilation units')

# store csv with all details

cunits.to_csv("docs/assets/tables/compile_units.csv")

# store csv with size by Module for v16

modules = cunits[['Module', COLUMN_SIZE_V16, COLUMN_SIZE_V201, COLUMN_SIZE_V16_V201]].groupby('Module').sum()
modules.loc['**Total**'] = [modules[COLUMN_SIZE_V16].sum(), modules[COLUMN_SIZE_V201].sum(), modules[COLUMN_SIZE_V16_V201].sum()]

print(modules)

modules.to_csv('docs/assets/tables/modules.csv')
