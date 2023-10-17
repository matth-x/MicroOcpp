// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef MO_MODEL_H
#define MO_MODEL_H

#include <memory>

#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Model/ConnectorBase/Connector.h>

namespace MicroOcpp {

class TransactionStore;
class SmartChargingService;
class ConnectorsCommon;
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
    std::vector<std::unique_ptr<Connector>> connectors;
    std::unique_ptr<TransactionStore> transactionStore;
    std::unique_ptr<SmartChargingService> smartChargingService;
    std::unique_ptr<ConnectorsCommon> chargeControlCommon;
    std::unique_ptr<MeteringService> meteringService;
    std::unique_ptr<FirmwareService> firmwareService;
    std::unique_ptr<DiagnosticsService> diagnosticsService;
    std::unique_ptr<HeartbeatService> heartbeatService;
    std::unique_ptr<AuthorizationService> authorizationService;
    std::unique_ptr<ReservationService> reservationService;
    std::unique_ptr<BootService> bootService;
    std::unique_ptr<ResetService> resetService;
    Clock clock;

    bool capabilitiesUpdated = true;
    void updateSupportedStandardProfiles();

    bool runTasks = false;

    const uint16_t bootNr = 0; //each boot of this lib has a unique number

public:
    Model(uint16_t bootNr = 0);
    Model(const Model& rhs) = delete;
    ~Model();

    void loop();

    void activateTasks() {runTasks = true;}

    void setTransactionStore(std::unique_ptr<TransactionStore> transactionStore);
    TransactionStore *getTransactionStore();

    void setSmartChargingService(std::unique_ptr<SmartChargingService> scs);
    SmartChargingService* getSmartChargingService() const;

    void setConnectorsCommon(std::unique_ptr<ConnectorsCommon> ccs);
    ConnectorsCommon *getConnectorsCommon();

    void setConnectors(std::vector<std::unique_ptr<Connector>>&& connectors);
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

    uint16_t getBootNr();
};

} //end namespace MicroOcpp

#endif
