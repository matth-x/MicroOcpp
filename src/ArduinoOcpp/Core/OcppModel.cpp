// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/Transactions/TransactionService.h>
#include <ArduinoOcpp/Tasks/SmartCharging/SmartChargingService.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Tasks/Metering/MeteringService.h>
#include <ArduinoOcpp/Tasks/FirmwareManagement/FirmwareService.h>
#include <ArduinoOcpp/Tasks/Diagnostics/DiagnosticsService.h>
#include <ArduinoOcpp/Tasks/Heartbeat/HeartbeatService.h>

#include <ArduinoOcpp/Debug.h>

using namespace ArduinoOcpp;

OcppModel::OcppModel(const OcppClock& system_clock)
        : ocppTime{system_clock} {
    
}

OcppModel::~OcppModel() = default;

void OcppModel::loop() {
    if (chargePointStatusService)
        chargePointStatusService->loop();
    
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
}

void OcppModel::setTransactionService(std::unique_ptr<TransactionService> ts) {
    transactionService = std::move(ts);
}

TransactionService *OcppModel::getTransactionService() {
    return transactionService.get();
}

void OcppModel::setSmartChargingService(std::unique_ptr<SmartChargingService> scs) {
    smartChargingService = std::move(scs);
}

SmartChargingService* OcppModel::getSmartChargingService() const {
    return smartChargingService.get();
}

void OcppModel::setChargePointStatusService(std::unique_ptr<ChargePointStatusService> cpss){
    chargePointStatusService = std::move(cpss);
}

ChargePointStatusService *OcppModel::getChargePointStatusService() const {
    return chargePointStatusService.get();
}

ConnectorStatus *OcppModel::getConnectorStatus(int connectorId) const {
    if (getChargePointStatusService() == nullptr) return nullptr;

    auto result = getChargePointStatusService()->getConnector(connectorId);
    if (result == nullptr) {
        AO_DBG_ERR("Cannot fetch connector with given connectorId. Return nullptr");
        //no error catch 
    }
    return result;
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

OcppTime& OcppModel::getOcppTime() {
    return ocppTime;
}
