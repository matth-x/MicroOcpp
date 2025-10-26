// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_MODEL_H
#define MO_MODEL_H

#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Model/Common/EvseId.h>
#include <MicroOcpp/Version.h>

namespace MicroOcpp {

class Context;

} //namespace MicroOcpp

#if MO_ENABLE_V16 || MO_ENABLE_V201

namespace MicroOcpp {

class BootService;
class HeartbeatService;
class RemoteControlService;

#if MO_ENABLE_DIAGNOSTICS
class DiagnosticsService;
#endif //MO_ENABLE_DIAGNOSTICS

#if MO_ENABLE_SMARTCHARGING
class SmartChargingService;
#endif //MO_ENABLE_SMARTCHARGING

#if MO_ENABLE_CERT_MGMT
class CertificateService;
#endif //MO_ENABLE_CERT_MGMT

#if MO_ENABLE_SECURITY_EVENT
class SecurityEventService;
#endif //MO_ENABLE_SECURITY_EVENT

//For Model modules which can be used with OCPP 1.6 and OCPP 2.0.1
class ModelCommon {
private:
    Context& context;

    BootService *bootService = nullptr;
    HeartbeatService *heartbeatService = nullptr;
    RemoteControlService *remoteControlService = nullptr;

#if MO_ENABLE_DIAGNOSTICS
    DiagnosticsService *diagnosticsService = nullptr;
#endif //MO_ENABLE_DIAGNOSTICS

#if MO_ENABLE_SMARTCHARGING
    SmartChargingService *smartChargingService = nullptr;
#endif //MO_ENABLE_SMARTCHARGING

#if MO_ENABLE_CERT_MGMT
    CertificateService *certService = nullptr;
#endif //MO_ENABLE_CERT_MGMT

#if MO_ENABLE_SECURITY_EVENT
    SecurityEventService *secEventService = nullptr;
#endif //MO_ENABLE_SECURITY_EVENT

protected:

    unsigned int numEvseId = MO_NUM_EVSEID;
    bool runTasks = false;

    ModelCommon(Context& context);
    ~ModelCommon();

    bool setupCommon();
    void loopCommon();

public:

    // Set number of EVSE IDs (including 0). On a charger with one physical connector, numEvseId is 2. Default value is MO_NUM_EVSEID
    void setNumEvseId(unsigned int numEvseId);
    unsigned int getNumEvseId();

    BootService *getBootService();
    HeartbeatService *getHeartbeatService();
    RemoteControlService *getRemoteControlService();

#if MO_ENABLE_DIAGNOSTICS
    DiagnosticsService *getDiagnosticsService();
#endif //MO_ENABLE_DIAGNOSTICS

#if MO_ENABLE_SMARTCHARGING
    SmartChargingService *getSmartChargingService();
#endif //MO_ENABLE_SMARTCHARGING

#if MO_ENABLE_CERT_MGMT
    CertificateService *getCertificateService();
#endif //MO_ENABLE_CERT_MGMT

#if MO_ENABLE_SECURITY_EVENT
    SecurityEventService *getSecurityEventService();
#endif //MO_ENABLE_SECURITY_EVENT

    void activateTasks() {runTasks = true;}

};

} //namespace MicroOcpp

#endif //MO_ENABLE_V16 || MO_ENABLE_V201

#if MO_ENABLE_V16

namespace MicroOcpp {
namespace v16 {

class ConfigurationService;
class TransactionService;
class MeteringService;
class ResetService;
class AvailabilityService;

#if MO_ENABLE_FIRMWAREMANAGEMENT
class FirmwareService;
#endif //MO_ENABLE_FIRMWAREMANAGEMENT

#if MO_ENABLE_LOCAL_AUTH
class AuthorizationService;
#endif //MO_ENABLE_LOCAL_AUTH

#if MO_ENABLE_RESERVATION
class ReservationService;
#endif //MO_ENABLE_RESERVATION

class Model : public MemoryManaged, public ModelCommon {
private:
    Context& context;

    ConfigurationService *configurationService = nullptr;
    TransactionService *transactionService = nullptr;
    MeteringService *meteringService = nullptr;
    ResetService *resetService = nullptr;
    AvailabilityService *availabilityService = nullptr;

#if MO_ENABLE_FIRMWAREMANAGEMENT
    FirmwareService *firmwareService = nullptr;
#endif //MO_ENABLE_FIRMWAREMANAGEMENT

#if MO_ENABLE_LOCAL_AUTH
    AuthorizationService *authorizationService = nullptr;
#endif //MO_ENABLE_LOCAL_AUTH

#if MO_ENABLE_RESERVATION
    ReservationService *reservationService = nullptr;
#endif //MO_ENABLE_RESERVATION

    void updateSupportedStandardProfiles();

public:
    Model(Context& context);
    ~Model();

    ConfigurationService *getConfigurationService();
    TransactionService *getTransactionService();
    MeteringService *getMeteringService();
    ResetService *getResetService();
    AvailabilityService *getAvailabilityService();

#if MO_ENABLE_FIRMWAREMANAGEMENT
    FirmwareService *getFirmwareService();
#endif //MO_ENABLE_FIRMWAREMANAGEMENT

#if MO_ENABLE_LOCAL_AUTH
    AuthorizationService *getAuthorizationService();
#endif //MO_ENABLE_LOCAL_AUTH

#if MO_ENABLE_RESERVATION
    ReservationService *getReservationService();
#endif //MO_ENABLE_RESERVATION

    bool setup();
    void loop();
};

} //namespace v16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16

#if MO_ENABLE_V201

namespace MicroOcpp {
namespace v201 {

class VariableService;
class TransactionService;
class MeteringService;
class ResetService;
class AvailabilityService;

class Model : public MemoryManaged, public ModelCommon {
private:
    Context& context;

    VariableService *variableService = nullptr;
    TransactionService *transactionService = nullptr;
    MeteringService *meteringService = nullptr;
    ResetService *resetService = nullptr;
    AvailabilityService *availabilityService = nullptr;

public:
    Model(Context& context);
    ~Model();

    VariableService *getVariableService();
    TransactionService *getTransactionService();
    MeteringService *getMeteringService();
    ResetService *getResetService();
    AvailabilityService *getAvailabilityService();

    bool setup();
    void loop();
};

} //namespace v201
} //namespace MicroOcpp
#endif //MO_ENABLE_V201

#if MO_ENABLE_V16 && !MO_ENABLE_V201
namespace MicroOcpp {
using Model = v16::Model;
}
#elif !MO_ENABLE_V16 && MO_ENABLE_V201
namespace MicroOcpp {
using Model = v201::Model;
}
#endif

#endif
