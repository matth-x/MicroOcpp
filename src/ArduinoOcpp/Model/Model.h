// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef AO_MODEL_H
#define AO_MODEL_H

#include <memory>

#include <ArduinoOcpp/Core/Time.h>
#include <ArduinoOcpp/Model/ChargeControl/Connector.h>

namespace ArduinoOcpp {

class TransactionStore;
class SmartChargingService;
class ChargeControlCommon;
class MeteringService;
class FirmwareService;
class DiagnosticsService;
class HeartbeatService;
class AuthorizationService;
class ReservationService;
class BootService;
class ResetService;

class Model {
private:
    std::vector<Connector> connectors;
    std::unique_ptr<TransactionStore> transactionStore;
    std::unique_ptr<SmartChargingService> smartChargingService;
    std::unique_ptr<ChargeControlCommon> chargeControlCommon;
    std::unique_ptr<MeteringService> meteringService;
    std::unique_ptr<FirmwareService> firmwareService;
    std::unique_ptr<DiagnosticsService> diagnosticsService;
    std::unique_ptr<HeartbeatService> heartbeatService;
    std::unique_ptr<AuthorizationService> authorizationService;
    std::unique_ptr<ReservationService> reservationService;
    std::unique_ptr<BootService> bootService;
    std::unique_ptr<ResetService> resetService;
    Clock clock;

    bool runTasks = false;

public:
    Model();
    Model(const Model& rhs) = delete;
    ~Model();

    void loop();

    void activateTasks() {runTasks = true;}

    void setTransactionStore(std::unique_ptr<TransactionStore> transactionStore);
    TransactionStore *getTransactionStore();

    void setSmartChargingService(std::unique_ptr<SmartChargingService> scs);
    SmartChargingService* getSmartChargingService() const;

    void setChargeControlCommon(std::unique_ptr<ChargeControlCommon> ccs);
    ChargeControlCommon *getChargeControlCommon();

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

    Clock &getClock();
};

} //end namespace ArduinoOcpp

#endif
