// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Model.h>

#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Model/SmartCharging/SmartChargingService.h>
#include <MicroOcpp/Model/ConnectorBase/ConnectorService.h>
#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Model/Metering/MeterValuesV201.h>
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
#include <MicroOcpp/Model/RemoteControl/RemoteControlService.h>

#include <MicroOcpp/Core/Configuration.h>

#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

Model::Model(ProtocolVersion version, uint16_t bootNr) : MemoryManaged("Model"), connectors(makeVector<std::unique_ptr<Connector>>(getMemoryTag())), version(version), bootNr(bootNr) {

}

Model::~Model() = default;

void Model::setNumEvseId(unsigned int numEvseId) {
    if (numEvseId >= MO_NUM_EVSEID) {
        MO_DBG_ERR("invalid arg");
        return;
    }
    this->numEvseId = numEvseId;
}

unsigned int Model::getNumEvseId() {
    return numEvseId;
}

BootService *Model::getBootService() {
    if (!bootService) {
        bootService = new BootService(context);
    }
    return bootService;
}

HeartbeatService *Model::getHeartbeatService() {
    if (!heartbeatService) {
        heartbeatService = new HeartbeatService(context);
    }
    return heartbeatService;
}

ResetService *Model::getResetService() {
    if (!resetService) {
        resetService = new ResetService(context);
    }
    return resetService;
}

ConnectorService *Model::getConnectorService() {
    if (!connectorService) {
        connectorService = new ConnectorService(context);
    }
    return connectorService;
}

Connector *Model::getConnector(unsigned int evseId) {
    if (evseId >= numEvseId) {
        MO_DBG_ERR("connector with evseId %u does not exist", evseId);
        return nullptr;
    }

    if (!connectors) {
        connectors = MO_MALLOC(getMemoryTag(), numEvseId * sizeof(Connector*));
        if (connectors) {
            memset(connectors, 0, sizeof(*connectors));
        } else {
            return nullptr; //OOM
        }
    }

    if (!connectors[evseId]) {
        connectors[evseId] = new Connector(context);
    }
    return connectors[evseId];
}

TransactionStoreEvse *Model::getTransactionStoreEvse(unsigned int evseId) {
    if (evseId >= numEvseId) {
        MO_DBG_ERR("connector with evseId %u does not exist", evseId);
        return nullptr;
    }

    if (!transactionStoreEvse) {
        transactionStoreEvse = MO_MALLOC(getMemoryTag(), numEvseId * sizeof(TransactionStoreEvse*));
        if (transactionStoreEvse) {
            memset(transactionStoreEvse, 0, sizeof(*transactionStoreEvse));
        } else {
            return nullptr; //OOM
        }
    }

    if (!transactionStoreEvse[evseId]) {
        transactionStoreEvse[evseId] = new TransactionStoreEvse(context);
    }
    return transactionStoreEvse[evseId];
}

MeteringService* Model::getMeteringService() {
    if (!meteringService) {
        meteringService = new MeteringService(context);
    }
    return meteringService;
}

MeteringServiceEvse* Model::getMeteringServiceEvse(unsigned int evseId) {
    if (evseId >= numEvseId) {
        MO_DBG_ERR("connector with evseId %u does not exist", evseId);
        return nullptr;
    }

    if (!meteringServiceEvse) {
        meteringServiceEvse = MO_MALLOC(getMemoryTag(), numEvseId * sizeof(MeteringServiceEvse*));
        if (meteringServiceEvse) {
            memset(meteringServiceEvse, 0, sizeof(*meteringServiceEvse));
        } else {
            return nullptr; //OOM
        }
    }

    if (!meteringServiceEvse[evseId]) {
        meteringServiceEvse[evseId] = new MeteringServiceEvse(context);
    }
    return meteringServiceEvse[evseId];
}

#if MO_ENABLE_FIRMWAREMANAGEMENT
FirmwareService *Model::getFirmwareService() {
    if (!firmwareService) {
        firmwareService = new FirmwareService(context);
    }
    return firmwareService;
}

DiagnosticsService *Model::getDiagnosticsService() {
    if (!diagnosticsService) {
        diagnosticsService = new DiagnosticsService(context);
    }
    return diagnosticsService;
}
#endif //MO_ENABLE_FIRMWAREMANAGEMENT

#if MO_ENABLE_LOCAL_AUTH
AuthorizationService *Model::getAuthorizationService() {
    if (!authorizationService) {
        authorizationService = new AuthorizationService(context);
    }
    return authorizationService;
}
#endif //MO_ENABLE_LOCAL_AUTH

#if MO_ENABLE_RESERVATION
ReservationService *Model::getReservationService() {
    if (!reservationService) {
        reservationService = new ReservationService(context);
    }
    return reservationService;
}
#endif //MO_ENABLE_RESERVATION

#if MO_ENABLE_SMARTCHARGING
SmartChargingService* Model::getSmartChargingService() {
    if (!smartChargingService) {
        smartChargingService = new SmartChargingService(context);
    }
    return smartChargingService;
}

SmartChargingServiceEvse *Model::getSmartChargingServiceEvse(unsigned int evseId) {
        if (evseId < 1 || evseId >= numEvseId) {
        MO_DBG_ERR("connector with evseId %u does not exist", evseId);
        return nullptr;
    }

    if (!smartChargingServiceEvse) {
        smartChargingServiceEvse = MO_MALLOC(getMemoryTag(), numEvseId * sizeof(SmartChargingServiceEvse*));
        if (smartChargingServiceEvse) {
            memset(smartChargingServiceEvse, 0, sizeof(*smartChargingServiceEvse));
        } else {
            return nullptr; //OOM
        }
    }

    if (!smartChargingServiceEvse[evseId]) {
        smartChargingServiceEvse[evseId] = new SmartChargingServiceEvse(context);
    }
    return smartChargingServiceEvse[evseId];
}
#endif //MO_ENABLE_SMARTCHARGING

#if MO_ENABLE_CERT_MGMT
CertificateService *Model::getCertificateService() {
    if (!certService) {
        certService = new CertificateService(context);
    }
    return certService;
}
#endif //MO_ENABLE_CERT_MGMT

bool Model::setup() {
    if (!getBootService()) {
        return false; //OOM
    }
    getBootService()->setup();
    if (!getHeartbeatService()) {
        return false; //OOM
    }
    getHeartbeatService()->setup();
}

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

    if (connectorService)
        connectorService->loop();

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

void Model::setMeteringServiceV201(std::unique_ptr<Ocpp201::MeteringService> rs) {
    this->meteringServiceV201 = std::move(rs);
    capabilitiesUpdated = true;
}

Ocpp201::MeteringService *Model::getMeteringServiceV201() const {
    return meteringServiceV201.get();
}

void Model::setRemoteControlService(std::unique_ptr<RemoteControlService> rs) {
    remoteControlService = std::move(rs);
    capabilitiesUpdated = true;
}

RemoteControlService *Model::getRemoteControlService() const {
    return remoteControlService.get();
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

    if (connectorService &&
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
