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

#if MO_ENABLE_V16 || MO_ENABLE_V201

using namespace MicroOcpp;

ModelCommon::ModelCommon(Context& context) : context(context) {

}

ModelCommon::~ModelCommon() {
    delete bootService;
    bootService = nullptr;
    delete heartbeatService;
    heartbeatService = nullptr;
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

}

bool ModelCommon::setupCommon() {

    if (!getBootService() || !getBootService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    if (!getHeartbeatService() || !getHeartbeatService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    if (!getRemoteControlService() || !getRemoteControlService()->setup()) {
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

    return true;
}

void ModelCommon::loopCommon() {

    if (bootService) {
        bootService->loop();
    }

    if (!runTasks) {
        return;
    }

    if (heartbeatService) {
        heartbeatService->loop();
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

#if MO_ENABLE_SECURITY_EVENT
    if (secEventService) {
        secEventService->loop();
    }
#endif //MO_ENABLE_SECURITY_EVENT
}

void ModelCommon::setNumEvseId(unsigned int numEvseId) {
    if (numEvseId >= MO_NUM_EVSEID) {
        MO_DBG_ERR("invalid arg");
        return;
    }
    this->numEvseId = numEvseId;
}

unsigned int ModelCommon::getNumEvseId() {
    return numEvseId;
}

BootService *ModelCommon::getBootService() {
    if (!bootService) {
        bootService = new BootService(context);
    }
    return bootService;
}

HeartbeatService *ModelCommon::getHeartbeatService() {
    if (!heartbeatService) {
        heartbeatService = new HeartbeatService(context);
    }
    return heartbeatService;
}

RemoteControlService *ModelCommon::getRemoteControlService() {
    if (!remoteControlService) {
        remoteControlService = new RemoteControlService(context);
    }
    return remoteControlService;
}

#if MO_ENABLE_DIAGNOSTICS
DiagnosticsService *ModelCommon::getDiagnosticsService() {
    if (!diagnosticsService) {
        diagnosticsService = new DiagnosticsService(context);
    }
    return diagnosticsService;
}
#endif //MO_ENABLE_DIAGNOSTICS

#if MO_ENABLE_SMARTCHARGING
SmartChargingService* ModelCommon::getSmartChargingService() {
    if (!smartChargingService) {
        smartChargingService = new SmartChargingService(context);
    }
    return smartChargingService;
}
#endif //MO_ENABLE_SMARTCHARGING

#if MO_ENABLE_CERT_MGMT
CertificateService *ModelCommon::getCertificateService() {
    if (!certService) {
        certService = new CertificateService(context);
    }
    return certService;
}
#endif //MO_ENABLE_CERT_MGMT

#if MO_ENABLE_SECURITY_EVENT
SecurityEventService *ModelCommon::getSecurityEventService() {
    if (!secEventService) {
        secEventService = new SecurityEventService(context);
    }
    return secEventService;
}
#endif //MO_ENABLE_SECURITY_EVENT

#endif //MO_ENABLE_V16 || MO_ENABLE_V201

#if MO_ENABLE_V16

v16::Model::Model(Context& context) : MemoryManaged("v16.Model"), ModelCommon(context), context(context) {

}

v16::Model::~Model() {
    delete transactionService;
    transactionService = nullptr;
    delete meteringService;
    meteringService = nullptr;
    delete resetService;
    resetService = nullptr;
    delete availabilityService;
    availabilityService = nullptr;

#if MO_ENABLE_FIRMWAREMANAGEMENT
    delete firmwareService;
    firmwareService = nullptr;
#endif //MO_ENABLE_FIRMWAREMANAGEMENT

#if MO_ENABLE_LOCAL_AUTH
    delete authorizationService;
    authorizationService = nullptr;
#endif //MO_ENABLE_LOCAL_AUTH

#if MO_ENABLE_RESERVATION
    delete reservationService;
    reservationService = nullptr;
#endif //MO_ENABLE_RESERVATION

    delete configurationService;
    configurationService = nullptr;
}

void v16::Model::updateSupportedStandardProfiles() {

    auto supportedFeatureProfilesString =
        configurationService->declareConfiguration<const char*>("SupportedFeatureProfiles", "", MO_CONFIGURATION_VOLATILE, Mutability::ReadOnly);

    if (!supportedFeatureProfilesString) {
        MO_DBG_ERR("OOM");
        return;
    }

    auto buf = makeString(getMemoryTag(), supportedFeatureProfilesString->getString());

    if (transactionService &&
            availabilityService &&
            getRemoteControlService() &&
            getHeartbeatService() &&
            getBootService()) {
        if (!strstr(supportedFeatureProfilesString->getString(), "Core")) {
            if (!buf.empty()) buf += ',';
            buf += "Core";
        }
    }

    if (firmwareService ||
            getDiagnosticsService()) {
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

    if (getSmartChargingService()) {
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

v16::ConfigurationService *v16::Model::getConfigurationService() {
    if (!configurationService) {
        configurationService = new v16::ConfigurationService(context);

        // need extra init step so that other modules can declare configs before setup()
        if (configurationService && !configurationService->init()) {
            // configurationService cannot be inited
            delete configurationService;
            configurationService = nullptr;
        }
    }
    return configurationService;
}

v16::TransactionService *v16::Model::getTransactionService() {
    if (!transactionService) {
        transactionService = new TransactionService(context);
    }
    return transactionService;
}

v16::MeteringService* v16::Model::getMeteringService() {
    if (!meteringService) {
        meteringService = new MeteringService(context);
    }
    return meteringService;
}

v16::ResetService *v16::Model::getResetService() {
    if (!resetService) {
        resetService = new ResetService(context);
    }
    return resetService;
}

v16::AvailabilityService *v16::Model::getAvailabilityService() {
    if (!availabilityService) {
        availabilityService = new v16::AvailabilityService(context);
    }
    return availabilityService;
}

#if MO_ENABLE_FIRMWAREMANAGEMENT
v16::FirmwareService *v16::Model::getFirmwareService() {
    if (!firmwareService) {
        firmwareService = new FirmwareService(context);
    }
    return firmwareService;
}
#endif //MO_ENABLE_FIRMWAREMANAGEMENT

#if MO_ENABLE_LOCAL_AUTH
v16::AuthorizationService *v16::Model::getAuthorizationService() {
    if (!authorizationService) {
        authorizationService = new AuthorizationService(context);
    }
    return authorizationService;
}
#endif //MO_ENABLE_LOCAL_AUTH

#if MO_ENABLE_RESERVATION
v16::ReservationService *v16::Model::getReservationService() {
    if (!reservationService) {
        reservationService = new ReservationService(context);
    }
    return reservationService;
}
#endif //MO_ENABLE_RESERVATION

bool v16::Model::setup() {

    if (!setupCommon()) {
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

#if MO_ENABLE_FIRMWAREMANAGEMENT
    if (!getFirmwareService() || !getFirmwareService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }
#endif //MO_ENABLE_FIRMWAREMANAGEMENT

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

    // Ensure this is set up last. ConfigurationService::setup() loads the persistent config values from flash
    if (!getConfigurationService() || !getConfigurationService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    updateSupportedStandardProfiles();

    // Register remainder of operations which don't have dedicated service
    context.getMessageService().registerOperation("DataTransfer", [] (Context&) -> Operation* {
        return new v16::DataTransfer();});

    return true;
}

void v16::Model::loop() {

    loopCommon();

    if (!runTasks) {
        return;
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

#if MO_ENABLE_RESERVATION
    if (reservationService) {
        reservationService->loop();
    }
#endif //MO_ENABLE_RESERVATION
}

#endif //MO_ENABLE_V16

#if MO_ENABLE_V201

using namespace MicroOcpp;

v201::Model::Model(Context& context) : MemoryManaged("v201.Model"), ModelCommon(context), context(context) {

}

v201::Model::~Model() {
    delete transactionService;
    transactionService = nullptr;
    delete meteringService;
    meteringService = nullptr;
    delete resetService;
    resetService = nullptr;
    delete availabilityService;
    availabilityService = nullptr;
    delete variableService;
    variableService = nullptr;
}

v201::VariableService *v201::Model::getVariableService() {
    if (!variableService) {
        variableService = new v201::VariableService(context);

        // need extra init step so that other modules can declare variables before setup()
        if (variableService && !variableService->init()) {
            // variableService cannot be inited
            delete variableService;
            variableService = nullptr;
        }
    }
    return variableService;
}

v201::TransactionService *v201::Model::getTransactionService() {
    if (!transactionService) {
        transactionService = new TransactionService(context);
    }
    return transactionService;
}

v201::MeteringService *v201::Model::getMeteringService() {
    if (!meteringService) {
        meteringService = new v201::MeteringService(context);
    }
    return meteringService;
}

v201::ResetService *v201::Model::getResetService() {
    if (!resetService) {
        resetService = new v201::ResetService(context);
    }
    return resetService;
}

v201::AvailabilityService *v201::Model::getAvailabilityService() {
    if (!availabilityService) {
        availabilityService = new v201::AvailabilityService(context);
    }
    return availabilityService;
}

bool v201::Model::setup() {

    if (!setupCommon()) {
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

    if (!getResetService() || !getResetService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    // Ensure this is set up last. VariableService::setup() loads the persistent variable values from flash
    if (!getVariableService() || !getVariableService()->setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    return true;
}

void v201::Model::loop() {

    loopCommon();

    if (variableService) {
        variableService->loop();
    }

    if (!runTasks) {
        return;
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
}

#endif //MO_ENABLE_V201
