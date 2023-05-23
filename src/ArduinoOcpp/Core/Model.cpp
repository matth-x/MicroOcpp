// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/Transactions/TransactionStore.h>
#include <ArduinoOcpp/Tasks/SmartCharging/SmartChargingService.h>
#include <ArduinoOcpp/Tasks/ChargeControl/ChargeControlCommon.h>
#include <ArduinoOcpp/Tasks/Metering/MeteringService.h>
#include <ArduinoOcpp/Tasks/FirmwareManagement/FirmwareService.h>
#include <ArduinoOcpp/Tasks/Diagnostics/DiagnosticsService.h>
#include <ArduinoOcpp/Tasks/Heartbeat/HeartbeatService.h>
#include <ArduinoOcpp/Tasks/Authorization/AuthorizationService.h>
#include <ArduinoOcpp/Tasks/Reservation/ReservationService.h>
#include <ArduinoOcpp/Tasks/Boot/BootService.h>
#include <ArduinoOcpp/Tasks/Reset/ResetService.h>

#include <ArduinoOcpp/Debug.h>

using namespace ArduinoOcpp;

OcppModel::OcppModel(const OcppClock& system_clock)
        : ocppTime{system_clock} {
    
}

OcppModel::~OcppModel() = default;

void OcppModel::loop() {

    if (bootService) {
        bootService->loop();
    }

    if (!runTasks) {
        return;
    }

    for (auto& connector : connectors) {
        connector.loop();
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
    
    if (reservationService)
        reservationService->loop();
}

void OcppModel::setTransactionStore(std::unique_ptr<TransactionStore> ts) {
    transactionStore = std::move(ts);
}

TransactionStore *OcppModel::getTransactionStore() {
    return transactionStore.get();
}

void OcppModel::setSmartChargingService(std::unique_ptr<SmartChargingService> scs) {
    smartChargingService = std::move(scs);
}

SmartChargingService* OcppModel::getSmartChargingService() const {
    return smartChargingService.get();
}

void OcppModel::setChargeControlCommon(std::unique_ptr<ChargeControlCommon> ccs) {
    chargeControlCommon = std::move(ccs);
}

ChargeControlCommon *OcppModel::getChargeControlCommon() {
    return chargeControlCommon.get();
}

void OcppModel::setConnectors(std::vector<Connector>&& connectors) {
    this->connectors = std::move(connectors);
}

unsigned int OcppModel::getNumConnectors() const {
    return connectors.size();
}

Connector *OcppModel::getConnector(unsigned int connectorId) {
    if (connectorId >= connectors.size()) {
        AO_DBG_ERR("connector with connectorId %u does not exist", connectorId);
        return nullptr;
    }

    return &connectors[connectorId];
}

void OcppModel::setMeteringSerivce(std::unique_ptr<MeteringService> ms) {
    meteringService = std::move(ms);
}

MeteringService* OcppModel::getMeteringService() const {
    return meteringService.get();
}

void OcppModel::setFirmwareService(std::unique_ptr<FirmwareService> fws) {
    firmwareService = std::move(fws);
}

FirmwareService *OcppModel::getFirmwareService() const {
    return firmwareService.get();
}

void OcppModel::setDiagnosticsService(std::unique_ptr<DiagnosticsService> ds) {
    diagnosticsService = std::move(ds);
}

DiagnosticsService *OcppModel::getDiagnosticsService() const {
    return diagnosticsService.get();
}

void OcppModel::setHeartbeatService(std::unique_ptr<HeartbeatService> hs) {
    heartbeatService = std::move(hs);
}

void OcppModel::setAuthorizationService(std::unique_ptr<AuthorizationService> as) {
    authorizationService = std::move(as);
}

AuthorizationService *OcppModel::getAuthorizationService() {
    return authorizationService.get();
}

void OcppModel::setReservationService(std::unique_ptr<ReservationService> rs) {
    reservationService = std::move(rs);
}

ReservationService *OcppModel::getReservationService() {
    return reservationService.get();
}

void OcppModel::setBootService(std::unique_ptr<BootService> bs){
    bootService = std::move(bs);
}

BootService *OcppModel::getBootService() const {
    return bootService.get();
}

void OcppModel::setResetService(std::unique_ptr<ResetService> rs) {
    this->resetService = std::move(rs);
}

ResetService *OcppModel::getResetService() const {
    return resetService.get();
}

OcppTime& OcppModel::getOcppTime() {
    return ocppTime;
}
