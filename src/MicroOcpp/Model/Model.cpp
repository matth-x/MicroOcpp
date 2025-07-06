// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#include <MicroOcpp/Model/Model.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Configuration/ConfigurationService.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Model/SmartCharging/SmartChargingService.h>
#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Model/FirmwareManagement/FirmwareService.h>
#include <MicroOcpp/Model/Diagnostics/DiagnosticsService.h>
#include <MicroOcpp/Model/Heartbeat/HeartbeatService.h>
#include <MicroOcpp/Model/Authorization/AuthorizationService.h>
#include <MicroOcpp/Model/Reservation/ReservationService.h>
#include <MicroOcpp/Model/Boot/BootService.h>
#include <MicroOcpp/Model/Reset/ResetService.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Model/Transactions/TransactionService16.h>
#include <MicroOcpp/Model/Transactions/TransactionService201.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Model/Certificates/CertificateService.h>
#include <MicroOcpp/Model/Availability/AvailabilityService.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlService.h>
#include <MicroOcpp/Model/SecurityEvent/SecurityEventService.h>
#include <MicroOcpp/Operations/DataTransfer.h>
#include <MicroOcpp/Operations/TriggerMessage.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16

using namespace MicroOcpp;

Ocpp16::Model::Model(Context& context) : MemoryManaged("v16.Model"), context(context) {

}

Ocpp16::Model::~Model() {
    delete bootService;
    bootService = nullptr;
    delete heartbeatService;
    heartbeatService = nullptr;
    delete transactionService;
    transactionService = nullptr;
    delete meteringService;
    meteringService = nullptr;
    delete resetService;
    resetService = nullptr;
    delete availabilityService;
    availabilityService = nullptr;
    delete remoteControlService;
    remoteControlService = nullptr;

#if MO_ENABLE_FIRMWAREMANAGEMENT
    delete firmwareService;
    firmwareService = nullptr;
#endif //MO_ENABLE_FIRMWAREMANAGEMENT

#if MO_ENABLE_DIAGNOSTICS
    delete diagnosticsService;
    diagnosticsService = nullptr;
#endif //MO_ENABLE_DIAGNOSTICS

#if MO_ENABLE_LOCAL_AUTH
    delete authorizationService;
    authorizationService = nullptr;
#endif //MO_ENABLE_LOCAL_AUTH

#if MO_ENABLE_RESERVATION
    delete reservationService;
    reservationService = nullptr;
#endif //MO_ENABLE_RESERVATION

#if MO_ENABLE_SMARTCHARGING
    delete smartChargingService;
    smartChargingService = nullptr;
#endif //MO_ENABLE_SMARTCHARGING

#if MO_ENABLE_CERT_MGMT
    delete certService;
    certService = nullptr;
#endif //MO_ENABLE_CERT_MGMT

#if MO_ENABLE_SECURITY_EVENT
    delete secEventService;
    secEventService = nullptr;
#endif //MO_ENABLE_SECURITY_EVENT

    delete configurationService;
    configurationService = nullptr;
}

void Ocpp16::Model::updateSupportedStandardProfiles() {

    auto supportedFeatureProfilesString =
        configurationService->declareConfiguration<const char*>("SupportedFeatureProfiles", "", MO_CONFIGURATION_VOLATILE, Mutability::ReadOnly);

    if (!supportedFeatureProfilesString) {
        MO_DBG_ERR("OOM");
        return;
    }

    auto buf = makeString(getMemoryTag(), supportedFeatureProfilesString->getString());

    if (transactionService &&
            availabilityService &&
            remoteControlService &&
            heartbeatService &&
            bootService) {
        if (!strstr(supportedFeatureProfilesString->getString(), "Core")) {
            if (!buf.empty()) buf += ',';
            buf += "Core";
        }
    }

    if (firmwareService ||
            diagnosticsService) {
        if (!strstr(supportedFeatureProfilesString->getString(), "FirmwareManagement")) {
            if (!buf.empty()) buf += ',';
            buf += "FirmwareManagement";
        }
    }

#if MO_ENABLE_LOCAL_AUTH
    if (authorizationService) {
        if (!strstr(supportedFeatureProfilesString->getString(), "LocalAuthListManagement")) {
            if (!buf.empty()) buf += ',';
            buf += "LocalAuthListManagement";
        }
    }
#endif //MO_ENABLE_LOCAL_AUTH

#if MO_ENABLE_RESERVATION
    if (reservationService) {
        if (!strstr(supportedFeatureProfilesString->getString(), "Reservation")) {
            if (!buf.empty()) buf += ',';
            buf += "Reservation";
        }
    }
#endif //MO_ENABLE_RESERVATION

    if (smartChargingService) {
        if (!strstr(supportedFeatureProfilesString->getString(), "SmartCharging")) {
            if (!buf.empty()) buf += ',';
            buf += "SmartCharging";
        }
    }

    if (!strstr(supportedFeatureProfilesString->getString(), "RemoteTrigger")) {
        if (!buf.empty()) buf += ',';
        buf += "RemoteTrigger";
    }

    supportedFeatureProfilesString->setString(buf.c_str());

    MO_DBG_DEBUG("supported feature profiles: %s", buf.c_str());
}

void Ocpp16::Model::setNumEvseId(unsigned int numEvseId) {
    if (numEvseId >= MO_NUM_EVSEID) {
        MO_DBG_ERR("invalid arg");
        return;
    }
    this->numEvseId = numEvseId;
}

unsigned int Ocpp16::Model::getNumEvseId() {
    return numEvseId;
}

BootService *Ocpp16::Model::getBootService() {
    if (!bootService) {
        bootService = new BootService(context);
    }
    return bootService;
}

HeartbeatService *Ocpp16::Model::getHeartbeatService() {
    if (!heartbeatService) {
        heartbeatService = new HeartbeatService(context);
    }
    return heartbeatService;
}

Ocpp16::ConfigurationService *Ocpp16::Model::getConfigurationService() {
    if (!configurationService) {
        configurationService = new Ocpp16::ConfigurationService(context);

        // need extra init step so that other modules can declare configs before setup()
        if (configurationService && !configurationService->init()) {
            // configurationService cannot be inited
            delete configurationService;
            configurationService = nullptr;
        }
    }
    return configurationService;
}

Ocpp16::TransactionService *Ocpp16::Model::getTransactionService() {
    if (!transactionService) {
        transactionService = new TransactionService(context);
    }
    return transactionService;
}

Ocpp16::MeteringService* Ocpp16::Model::getMeteringService() {
    if (!meteringService) {
        meteringService = new MeteringService(context);
    }
    return meteringService;
}

Ocpp16::ResetService *Ocpp16::Model::getResetService() {
    if (!resetService) {
        resetService = new ResetService(context);
    }
    return resetService;
}

Ocpp16::AvailabilityService *Ocpp16::Model::getAvailabilityService() {
    if (!availabilityService) {
        availabilityService = new Ocpp16::AvailabilityService(context);
    }
    return availabilityService;
}

RemoteControlService *Ocpp16::Model::getRemoteControlService() {
    if (!remoteControlService) {
        remoteControlService = new RemoteControlService(context);
    }
    return remoteControlService;
}

#if MO_ENABLE_FIRMWAREMANAGEMENT
Ocpp16::FirmwareService *Ocpp16::Model::getFirmwareService() {
    if (!firmwareService) {
        firmwareService = new FirmwareService(context);
    }
    return firmwareService;
}
#endif //MO_ENABLE_FIRMWAREMANAGEMENT

#if MO_ENABLE_DIAGNOSTICS
DiagnosticsService *Ocpp16::Model::getDiagnosticsService() {
    if (!diagnosticsService) {
        diagnosticsService = new DiagnosticsService(context);
    }
    return diagnosticsService;
}
#endif //MO_ENABLE_DIAGNOSTICS

#if MO_ENABLE_LOCAL_AUTH
Ocpp16::AuthorizationService *Ocpp16::Model::getAuthorizationService() {
    if (!authorizationService) {
        authorizationService = new AuthorizationService(context);
    }
    return authorizationService;
}
#endif //MO_ENABLE_LOCAL_AUTH

#if MO_ENABLE_RESERVATION
Ocpp16::ReservationService *Ocpp16::Model::getReservationService() {
    if (!reservationService) {
        reservationService = new ReservationService(context);
    }
    return reservationService;
}
#endif //MO_ENABLE_RESERVATION

#if MO_ENABLE_SMARTCHARGING
SmartChargingService* Ocpp16::Model::getSmartChargingService() {
    if (!smartChargingService) {
        smartChargingService = new SmartChargingService(context);
    }
    return smartChargingService;
}
#endif //MO_ENABLE_SMARTCHARGING

#if MO_ENABLE_CERT_MGMT
CertificateService *Ocpp16::Model::getCertificateService() {
    if (!certService) {
        certService = new CertificateService(context);
    }
    return certService;
}
#endif //MO_ENABLE_CERT_MGMT

#if MO_ENABLE_SECURITY_EVENT
SecurityEventService *Ocpp16::Model::getSecurityEventService() {
    if (!secEventService) {
        secEventService = new SecurityEventService(context);
    }
    return secEventService;
}
#endif //MO_ENABLE_SECURITY_EVENT

bool Ocpp16::Model::setup() {
    if (!getBootService() || !getBootService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    if (!getHeartbeatService() || !getHeartbeatService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    if (!getTransactionService() || !getTransactionService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    if (!getMeteringService() || !getMeteringService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }
    
    if (!getResetService() || !getResetService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    if (!getAvailabilityService() || !getAvailabilityService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    if (!getRemoteControlService() || !getRemoteControlService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }

#if MO_ENABLE_FIRMWAREMANAGEMENT
    if (!getFirmwareService() || !getFirmwareService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }
#endif //MO_ENABLE_FIRMWAREMANAGEMENT

#if MO_ENABLE_DIAGNOSTICS
    if (!getDiagnosticsService() || !getDiagnosticsService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }
#endif //MO_ENABLE_DIAGNOSTICS

#if MO_ENABLE_LOCAL_AUTH
    if (!getAuthorizationService() || !getAuthorizationService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }
#endif //MO_ENABLE_LOCAL_AUTH

#if MO_ENABLE_RESERVATION
    if (!getReservationService() || !getReservationService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }
#endif //MO_ENABLE_RESERVATION

#if MO_ENABLE_SMARTCHARGING
    if (!getSmartChargingService() || !getSmartChargingService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }
#endif //MO_ENABLE_SMARTCHARGING

#if MO_ENABLE_CERT_MGMT
    if (!getCertificateService() || !getCertificateService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }
#endif //MO_ENABLE_CERT_MGMT

#if MO_ENABLE_SECURITY_EVENT
    if (!getSecurityEventService() || !getSecurityEventService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }
#endif //MO_ENABLE_SECURITY_EVENT

    // Ensure this is set up last. ConfigurationService::setup() loads the persistent config values from flash
    if (!getConfigurationService() || !getConfigurationService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    updateSupportedStandardProfiles();

    // Register remainder of operations which don't have dedicated service
    context.getMessageService().registerOperation("DataTransfer", [] (Context&) -> Operation* {
        return new Ocpp16::DataTransfer();});
    
    return true;
}

void Ocpp16::Model::loop() {

    if (bootService) {
        bootService->loop();
    }

    if (!runTasks) {
        return;
    }

    if (heartbeatService) {
        heartbeatService->loop();
    }

    if (transactionService) {
        transactionService->loop();
    }

    if (meteringService) {
        meteringService->loop();
    }

    if (resetService) {
        resetService->loop();
    }

    if (availabilityService) {
        availabilityService->loop();
    }

#if MO_ENABLE_FIRMWAREMANAGEMENT
    if (firmwareService) {
        firmwareService->loop();
    }
#endif //MO_ENABLE_FIRMWAREMANAGEMENT

#if MO_ENABLE_DIAGNOSTICS
    if (diagnosticsService) {
        diagnosticsService->loop();
    }
#endif //MO_ENABLE_DIAGNOSTICS

#if MO_ENABLE_RESERVATION
    if (reservationService) {
        reservationService->loop();
    }
#endif //MO_ENABLE_RESERVATION

#if MO_ENABLE_SMARTCHARGING
    if (smartChargingService) {
        smartChargingService->loop();
    }
#endif //MO_ENABLE_SMARTCHARGING
}

#endif //MO_ENABLE_V16

#if MO_ENABLE_V201

using namespace MicroOcpp;

Ocpp201::Model::Model(Context& context) : MemoryManaged("v201.Model"), context(context) {

}

Ocpp201::Model::~Model() {
    delete bootService;
    bootService = nullptr;
    delete heartbeatService;
    heartbeatService = nullptr;
    delete transactionService;
    transactionService = nullptr;
    delete meteringService;
    meteringService = nullptr;
    delete resetService;
    resetService = nullptr;
    delete availabilityService;
    availabilityService = nullptr;
    delete remoteControlService;
    remoteControlService = nullptr;

#if MO_ENABLE_DIAGNOSTICS
    delete diagnosticsService;
    diagnosticsService = nullptr;
#endif //MO_ENABLE_DIAGNOSTICS

#if MO_ENABLE_SMARTCHARGING
    delete smartChargingService;
    smartChargingService = nullptr;
#endif //MO_ENABLE_SMARTCHARGING

#if MO_ENABLE_CERT_MGMT
    delete certService;
    certService = nullptr;
#endif //MO_ENABLE_CERT_MGMT

#if MO_ENABLE_SECURITY_EVENT
    delete secEventService;
    secEventService = nullptr;
#endif //MO_ENABLE_SECURITY_EVENT

    delete variableService;
    variableService = nullptr;
}

void Ocpp201::Model::setNumEvseId(unsigned int numEvseId) {
    if (numEvseId >= MO_NUM_EVSEID) {
        MO_DBG_ERR("invalid arg");
        return;
    }
    this->numEvseId = numEvseId;
}

unsigned int Ocpp201::Model::getNumEvseId() {
    return numEvseId;
}

BootService *Ocpp201::Model::getBootService() {
    if (!bootService) {
        bootService = new BootService(context);
    }
    return bootService;
}

HeartbeatService *Ocpp201::Model::getHeartbeatService() {
    if (!heartbeatService) {
        heartbeatService = new HeartbeatService(context);
    }
    return heartbeatService;
}

Ocpp201::VariableService *Ocpp201::Model::getVariableService() {
    if (!variableService) {
        variableService = new Ocpp201::VariableService(context);

        // need extra init step so that other modules can declare variables before setup()
        if (variableService && !variableService->init()) {
            // variableService cannot be inited
            delete variableService;
            variableService = nullptr;
        }
    }
    return variableService;
}

Ocpp201::TransactionService *Ocpp201::Model::getTransactionService() {
    if (!transactionService) {
        transactionService = new TransactionService(context);
    }
    return transactionService;
}

Ocpp201::MeteringService *Ocpp201::Model::getMeteringService() {
    if (!meteringService) {
        meteringService = new Ocpp201::MeteringService(context);
    }
    return meteringService;
}

Ocpp201::ResetService *Ocpp201::Model::getResetService() {
    if (!resetService) {
        resetService = new Ocpp201::ResetService(context);
    }
    return resetService;
}

Ocpp201::AvailabilityService *Ocpp201::Model::getAvailabilityService() {
    if (!availabilityService) {
        availabilityService = new Ocpp201::AvailabilityService(context);
    }
    return availabilityService;
}

RemoteControlService *Ocpp201::Model::getRemoteControlService() {
    if (!remoteControlService) {
        remoteControlService = new RemoteControlService(context);
    }
    return remoteControlService;
}

#if MO_ENABLE_DIAGNOSTICS
DiagnosticsService *Ocpp201::Model::getDiagnosticsService() {
    if (!diagnosticsService) {
        diagnosticsService = new DiagnosticsService(context);
    }
    return diagnosticsService;
}
#endif //MO_ENABLE_DIAGNOSTICS

#if MO_ENABLE_SMARTCHARGING
SmartChargingService* Ocpp201::Model::getSmartChargingService() {
    if (!smartChargingService) {
        smartChargingService = new SmartChargingService(context);
    }
    return smartChargingService;
}
#endif //MO_ENABLE_SMARTCHARGING

#if MO_ENABLE_CERT_MGMT
CertificateService *Ocpp201::Model::getCertificateService() {
    if (!certService) {
        certService = new CertificateService(context);
    }
    return certService;
}
#endif //MO_ENABLE_CERT_MGMT

#if MO_ENABLE_SECURITY_EVENT
SecurityEventService *Ocpp201::Model::getSecurityEventService() {
    if (!secEventService) {
        secEventService = new SecurityEventService(context);
    }
    return secEventService;
}
#endif //MO_ENABLE_SECURITY_EVENT

bool Ocpp201::Model::setup() {
    if (!getBootService() || !getBootService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    if (!getHeartbeatService() || !getHeartbeatService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    if (!getTransactionService() || !getTransactionService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    if (!getMeteringService() || !getMeteringService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }
    if (!getAvailabilityService() || !getAvailabilityService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    if (!getRemoteControlService() || !getRemoteControlService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    if (!getResetService() || !getResetService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }

#if MO_ENABLE_DIAGNOSTICS
    if (!getDiagnosticsService() || !getDiagnosticsService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }
#endif //MO_ENABLE_DIAGNOSTICS

#if MO_ENABLE_SMARTCHARGING
    if (!getSmartChargingService() || !getSmartChargingService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }
#endif //MO_ENABLE_SMARTCHARGING

#if MO_ENABLE_CERT_MGMT
    if (!getCertificateService() || !getCertificateService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }
#endif //MO_ENABLE_CERT_MGMT

#if MO_ENABLE_SECURITY_EVENT
    if (!getSecurityEventService() || !getSecurityEventService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }
#endif //MO_ENABLE_SECURITY_EVENT

    // Ensure this is set up last. VariableService::setup() loads the persistent variable values from flash
    if (!getVariableService() || !getVariableService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    return true;
}

void Ocpp201::Model::loop() {

    if (bootService) {
        bootService->loop();
    }

    if (variableService) {
        variableService->loop();
    }

    if (!runTasks) {
        return;
    }

    if (heartbeatService) {
        heartbeatService->loop();
    }

    if (transactionService) {
        transactionService->loop();
    }

    if (resetService) {
        resetService->loop();
    }

    if (availabilityService) {
        availabilityService->loop();
    }

#if MO_ENABLE_DIAGNOSTICS
    if (diagnosticsService) {
        diagnosticsService->loop();
    }
#endif //MO_ENABLE_DIAGNOSTICS

#if MO_ENABLE_SMARTCHARGING
    if (smartChargingService) {
        smartChargingService->loop();
    }
#endif //MO_ENABLE_SMARTCHARGING
}

#endif //MO_ENABLE_V201
