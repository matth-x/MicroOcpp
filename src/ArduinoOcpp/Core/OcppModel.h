// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef OCPPMODEL_H
#define OCPPMODEL_H

#include <memory>

#include <ArduinoOcpp/Core/OcppTime.h>
#include <ArduinoOcpp/Tasks/ChargeControl/Connector.h>

namespace ArduinoOcpp {

class TransactionStore;
class SmartChargingService;
class ChargeControlService;
class MeteringService;
class FirmwareService;
class DiagnosticsService;
class HeartbeatService;
class AuthorizationService;
class ReservationService;
class BootService;
class ResetService;

class OcppModel {
private:
    std::vector<Connector> connectors;
    std::unique_ptr<TransactionStore> transactionStore;
    std::unique_ptr<SmartChargingService> smartChargingService;
    std::unique_ptr<ChargeControlService> chargeControlService;
    std::unique_ptr<MeteringService> meteringService;
    std::unique_ptr<FirmwareService> firmwareService;
    std::unique_ptr<DiagnosticsService> diagnosticsService;
    std::unique_ptr<HeartbeatService> heartbeatService;
    std::unique_ptr<AuthorizationService> authorizationService;
    std::unique_ptr<ReservationService> reservationService;
    std::unique_ptr<BootService> bootService;
    std::unique_ptr<ResetService> resetService;
    OcppTime ocppTime;

    bool runTasks = false;

public:
    OcppModel(const OcppClock& system_clock);
    OcppModel() = delete;
    OcppModel(const OcppModel& rhs) = delete;
    ~OcppModel();

    void loop();

    void activateTasks() {runTasks = true;}

    void setTransactionStore(std::unique_ptr<TransactionStore> transactionStore);
    TransactionStore *getTransactionStore();

    void setSmartChargingService(std::unique_ptr<SmartChargingService> scs);
    SmartChargingService* getSmartChargingService() const;

    void setChargeControlService(std::unique_ptr<ChargeControlService> ccs);
    ChargeControlService *getChargeControlService();

    void setConnectors(std::vector<Connector>&& connectors);
    unsigned int getNumConnectors() const;
    Connector *getConnector(unsigned int connectorId);

    void setMeteringSerivce(std::unique_ptr<MeteringService> meteringService);
    MeteringService* getMeteringService() const;

    void setFirmwareService(std::unique_ptr<FirmwareService> firmwareService);
    FirmwareService *getFirmwareService() const;

    void setDiagnosticsService(std::unique_ptr<DiagnosticsService> diagnosticsService);
    DiagnosticsService *getDiagnosticsService() const;

    void setHeartbeatService(std::unique_ptr<HeartbeatService> heartbeatService);

    void setAuthorizationService(std::unique_ptr<AuthorizationService> authorizationService);
    AuthorizationService *getAuthorizationService();

    void setReservationService(std::unique_ptr<ReservationService> reservationService);
    ReservationService *getReservationService();

    void setBootService(std::unique_ptr<BootService> bs);
    BootService *getBootService() const;

    void setResetService(std::unique_ptr<ResetService> rs);
    ResetService *getResetService() const;

    OcppTime &getOcppTime();
};

} //end namespace ArduinoOcpp

#endif
