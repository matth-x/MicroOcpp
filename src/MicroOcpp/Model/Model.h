// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_MODEL_H
#define MO_MODEL_H

#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Version.h>

// number of EVSE IDs (including 0). On a charger with one physical connector, NUM_EVSEID is 2
#ifndef MO_NUM_EVSEID
// Use MO_NUMCONNECTORS if defined, for backwards compatibility
#if defined(MO_NUMCONNECTORS)
#define MO_NUM_EVSEID MO_NUMCONNECTORS
#else
#define MO_NUM_EVSEID 2
#endif
#endif // MO_NUM_EVSEID

namespace MicroOcpp {

class Context;

class BootService;
class HeartbeatService;
class ResetService;
class ConnectorService;
class Connector;
class TransactionStoreEvse;
class MeteringService;
class MeteringServiceEvse;

#if MO_ENABLE_FIRMWAREMANAGEMENT
class FirmwareService;
class DiagnosticsService;
#endif //MO_ENABLE_FIRMWAREMANAGEMENT

#if MO_ENABLE_LOCAL_AUTH
class AuthorizationService;
#endif //MO_ENABLE_LOCAL_AUTH

#if MO_ENABLE_RESERVATION
class ReservationService;
#endif //MO_ENABLE_RESERVATION

#if MO_ENABLE_SMARTCHARGING
class SmartChargingService;
class SmartChargingServiceEvse;
#endif //MO_ENABLE_SMARTCHARGING

#if MO_ENABLE_CERT_MGMT
class CertificateService;
#endif //MO_ENABLE_CERT_MGMT

#if MO_ENABLE_V201
class AvailabilityService;
class VariableService;
class TransactionService;
class RemoteControlService;

namespace Ocpp201 {
class ResetService;
class MeteringService;
}
#endif //MO_ENABLE_V201

class Model : public MemoryManaged {
private:
    Context& context;

    BootService *bootService = nullptr;
    HeartbeatService *heartbeatService = nullptr;
    ResetService *resetService = nullptr;
    ConnectorService *connectorService = nullptr;
    Connector **connectors = nullptr;
    TransactionStoreEvse **transactionStoreEvse = nullptr;
    MeteringService *meteringService = nullptr;
    MeteringServiceEvse **meteringServiceEvse = nullptr;

#if MO_ENABLE_FIRMWAREMANAGEMENT
    FirmwareService *firmwareService = nullptr;
    DiagnosticsService *diagnosticsService = nullptr;
#endif //MO_ENABLE_FIRMWAREMANAGEMENT

#if MO_ENABLE_LOCAL_AUTH
    AuthorizationService *authorizationService = nullptr;
#endif //MO_ENABLE_LOCAL_AUTH

#if MO_ENABLE_RESERVATION
    ReservationService *reservationService = nullptr;
#endif //MO_ENABLE_RESERVATION

#if MO_ENABLE_SMARTCHARGING
    SmartChargingService *smartChargingService = nullptr;
    SmartChargingServiceEvse **smartChargingServiceEvse = nullptr;
#endif //MO_ENABLE_SMARTCHARGING

#if MO_ENABLE_CERT_MGMT
    CertificateService *certService = nullptr;
#endif //MO_ENABLE_CERT_MGMT

#if MO_ENABLE_V201
    AvailabilityService *availabilityService = nullptr;
    VariableService *variableService = nullptr;
    TransactionService *transactionService = nullptr;
    Ocpp201::ResetService *resetServiceV201 = nullptr;
    Ocpp201::MeteringService *meteringServiceV201 = nullptr;
    RemoteControlService *remoteControlService = nullptr;
#endif

    Clock clock;

    int ocppVersion = MO_OCPP_V16;
    unsigned int numEvseId = MO_NUM_EVSEID;

    bool capabilitiesUpdated = true;
    void updateSupportedStandardProfiles();

    bool runTasks = false;
    bool isSetup = false;

    const uint16_t bootNr = 0; //each boot of this lib has a unique number

public:
    Model(ProtocolVersion version = ProtocolVersion(1,6), uint16_t bootNr = 0);
    Model(const Model& rhs) = delete;
    ~Model();

    // Set number of EVSE IDs (including 0). On a charger with one physical connector, numEvseId is 2. Default value is MO_NUM_EVSEID
    void setNumEvseId(unsigned int numEvseId);
    unsigned int getNumEvseId();

    BootService *getBootService();
    HeartbeatService *getHeartbeatService();
    ResetService *getResetService();
    ConnectorService *getConnectorService();
    Connector *getConnector(unsigned int evseId);
    TransactionStoreEvse *getTransactionStoreEvse();
    MeteringService *getMeteringService();
    MeteringServiceEvse *getMeteringServiceEvse(unsigned int evseId);

#if MO_ENABLE_FIRMWAREMANAGEMENT
    FirmwareService *getFirmwareService();
    DiagnosticsService *getDiagnosticsService();
#endif //MO_ENABLE_FIRMWAREMANAGEMENT

#if MO_ENABLE_LOCAL_AUTH
    AuthorizationService *getAuthorizationService();
#endif //MO_ENABLE_LOCAL_AUTH

#if MO_ENABLE_RESERVATION
    ReservationService *getReservationService();
#endif //MO_ENABLE_RESERVATION

#if MO_ENABLE_SMARTCHARGING
    SmartChargingService *getSmartChargingService();
    SmartChargingServiceEvse *getSmartChargingServiceEvse(unsigned int evseId);
#endif //MO_ENABLE_SMARTCHARGING

#if MO_ENABLE_CERT_MGMT
    CertificateService *getCertificateService() const;
#endif //MO_ENABLE_CERT_MGMT

#if MO_ENABLE_V201
    void setAvailabilityService(std::unique_ptr<AvailabilityService> as);
    AvailabilityService *getAvailabilityService() const;

    void setVariableService(std::unique_ptr<VariableService> vs);
    VariableService *getVariableService() const;

    void setTransactionService(std::unique_ptr<TransactionService> ts);
    TransactionService *getTransactionService() const;

    void setResetServiceV201(std::unique_ptr<Ocpp201::ResetService> rs);
    Ocpp201::ResetService *getResetServiceV201() const;

    void setMeteringServiceV201(std::unique_ptr<Ocpp201::MeteringService> ms);
    Ocpp201::MeteringService *getMeteringServiceV201() const;

    void setRemoteControlService(std::unique_ptr<RemoteControlService> rs);
    RemoteControlService *getRemoteControlService() const;
#endif

    bool setup(int ocppVersion);

    void loop();

    void activateTasks() {runTasks = true;}

    Clock &getClock();

    const ProtocolVersion& getVersion() const;

    uint16_t getBootNr();
};

} //end namespace MicroOcpp

#endif
