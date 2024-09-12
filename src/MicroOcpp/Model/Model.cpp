// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Model.h>

#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Model/SmartCharging/SmartChargingService.h>
#include <MicroOcpp/Model/ConnectorBase/ConnectorsCommon.h>
#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Model/FirmwareManagement/FirmwareService.h>
#include <MicroOcpp/Model/Diagnostics/DiagnosticsService.h>
#include <MicroOcpp/Model/Heartbeat/HeartbeatService.h>
#include <MicroOcpp/Model/Authorization/AuthorizationService.h>
#include <MicroOcpp/Model/Reservation/ReservationService.h>
#include <MicroOcpp/Model/Boot/BootService.h>
#include <MicroOcpp/Model/Reset/ResetService.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Model/Transactions/TransactionService.h>
#include <MicroOcpp/Model/Certificates/CertificateService.h>
#include <MicroOcpp/Model/Availability/AvailabilityService.h>

#include <MicroOcpp/Core/Configuration.h>

#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

Model::Model(ProtocolVersion version, uint16_t bootNr) : MemoryManaged("Model"), connectors(makeVector<std::unique_ptr<Connector>>(getMemoryTag())), version(version), bootNr(bootNr) {

}

Model::~Model() = default;

void Model::loop() {

    if (bootService) {
        bootService->loop();
    }

    if (capabilitiesUpdated) {
        updateSupportedStandardProfiles();
        capabilitiesUpdated = false;
    }

    if (!runTasks) {
        return;
    }

    for (auto& connector : connectors) {
        connector->loop();
    }

    if (chargeControlCommon)
        chargeControlCommon->loop();

    if (smartChargingService)
        smartChargingService->loop();

    if (heartbeatService)
        heartbeatService->loop();

    if (meteringService)
        meteringService->loop();

    if (diagnosticsService)
        diagnosticsService->loop();

    if (firmwareService)
        firmwareService->loop();

#if MO_ENABLE_RESERVATION
    if (reservationService)
        reservationService->loop();
#endif //MO_ENABLE_RESERVATION

    if (resetService)
        resetService->loop();

#if MO_ENABLE_V201
    if (availabilityService)
        availabilityService->loop();

    if (transactionService)
        transactionService->loop();
    
    if (resetServiceV201)
        resetServiceV201->loop();
#endif
}

void Model::setTransactionStore(std::unique_ptr<TransactionStore> ts) {
    transactionStore = std::move(ts);
    capabilitiesUpdated = true;
}

TransactionStore *Model::getTransactionStore() {
    return transactionStore.get();
}

void Model::setSmartChargingService(std::unique_ptr<SmartChargingService> scs) {
    smartChargingService = std::move(scs);
    capabilitiesUpdated = true;
}

SmartChargingService* Model::getSmartChargingService() const {
    return smartChargingService.get();
}

void Model::setConnectorsCommon(std::unique_ptr<ConnectorsCommon> ccs) {
    chargeControlCommon = std::move(ccs);
    capabilitiesUpdated = true;
}

ConnectorsCommon *Model::getConnectorsCommon() {
    return chargeControlCommon.get();
}

void Model::setConnectors(Vector<std::unique_ptr<Connector>>&& connectors) {
    this->connectors = std::move(connectors);
    capabilitiesUpdated = true;
}

unsigned int Model::getNumConnectors() const {
    return connectors.size();
}

Connector *Model::getConnector(unsigned int connectorId) {
    if (connectorId >= connectors.size()) {
        MO_DBG_ERR("connector with connectorId %u does not exist", connectorId);
        return nullptr;
    }

    return connectors[connectorId].get();
}

void Model::setMeteringSerivce(std::unique_ptr<MeteringService> ms) {
    meteringService = std::move(ms);
    capabilitiesUpdated = true;
}

MeteringService* Model::getMeteringService() const {
    return meteringService.get();
}

void Model::setFirmwareService(std::unique_ptr<FirmwareService> fws) {
    firmwareService = std::move(fws);
    capabilitiesUpdated = true;
}

FirmwareService *Model::getFirmwareService() const {
    return firmwareService.get();
}

void Model::setDiagnosticsService(std::unique_ptr<DiagnosticsService> ds) {
    diagnosticsService = std::move(ds);
    capabilitiesUpdated = true;
}

DiagnosticsService *Model::getDiagnosticsService() const {
    return diagnosticsService.get();
}

void Model::setHeartbeatService(std::unique_ptr<HeartbeatService> hs) {
    heartbeatService = std::move(hs);
    capabilitiesUpdated = true;
}

#if MO_ENABLE_LOCAL_AUTH
void Model::setAuthorizationService(std::unique_ptr<AuthorizationService> as) {
    authorizationService = std::move(as);
    capabilitiesUpdated = true;
}

AuthorizationService *Model::getAuthorizationService() {
    return authorizationService.get();
}
#endif //MO_ENABLE_LOCAL_AUTH

#if MO_ENABLE_RESERVATION
void Model::setReservationService(std::unique_ptr<ReservationService> rs) {
    reservationService = std::move(rs);
    capabilitiesUpdated = true;
}

ReservationService *Model::getReservationService() {
    return reservationService.get();
}
#endif //MO_ENABLE_RESERVATION

void Model::setBootService(std::unique_ptr<BootService> bs){
    bootService = std::move(bs);
    capabilitiesUpdated = true;
}

BootService *Model::getBootService() const {
    return bootService.get();
}

void Model::setResetService(std::unique_ptr<ResetService> rs) {
    this->resetService = std::move(rs);
    capabilitiesUpdated = true;
}

ResetService *Model::getResetService() const {
    return resetService.get();
}

#if MO_ENABLE_CERT_MGMT
void Model::setCertificateService(std::unique_ptr<CertificateService> cs) {
    this->certService = std::move(cs);
    capabilitiesUpdated = true;
}

CertificateService *Model::getCertificateService() const {
    return certService.get();
}
#endif //MO_ENABLE_CERT_MGMT

#if MO_ENABLE_V201
void Model::setAvailabilityService(std::unique_ptr<AvailabilityService> as) {
    this->availabilityService = std::move(as);
    capabilitiesUpdated = true;
}

AvailabilityService *Model::getAvailabilityService() const {
    return availabilityService.get();
}

void Model::setVariableService(std::unique_ptr<VariableService> vs) {
    this->variableService = std::move(vs);
    capabilitiesUpdated = true;
}

VariableService *Model::getVariableService() const {
    return variableService.get();
}

void Model::setTransactionService(std::unique_ptr<TransactionService> ts) {
    this->transactionService = std::move(ts);
    capabilitiesUpdated = true;
}

TransactionService *Model::getTransactionService() const {
    return transactionService.get();
}

void Model::setResetServiceV201(std::unique_ptr<Ocpp201::ResetService> rs) {
    this->resetServiceV201 = std::move(rs);
    capabilitiesUpdated = true;
}

Ocpp201::ResetService *Model::getResetServiceV201() const {
    return resetServiceV201.get();
}
#endif

Clock& Model::getClock() {
    return clock;
}

const ProtocolVersion& Model::getVersion() const {
    return version;
}

uint16_t Model::getBootNr() {
    return bootNr;
}

void Model::updateSupportedStandardProfiles() {

    auto supportedFeatureProfilesString =
        declareConfiguration<const char*>("SupportedFeatureProfiles", "", CONFIGURATION_VOLATILE, true);
    
    if (!supportedFeatureProfilesString) {
        MO_DBG_ERR("OOM");
        return;
    }

    auto buf = makeString(getMemoryTag(), supportedFeatureProfilesString->getString());

    if (chargeControlCommon &&
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
