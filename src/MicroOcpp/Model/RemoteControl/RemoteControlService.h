// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_REMOTECONTROLSERVICE_H
#define MO_REMOTECONTROLSERVICE_H

#include <MicroOcpp/Model/RemoteControl/RemoteControlDefs.h>
#include <MicroOcpp/Model/SmartCharging/SmartChargingModel.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Model/Common/EvseId.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16 || MO_ENABLE_V201

namespace MicroOcpp {

class Context;
class RemoteControlService;

#if MO_ENABLE_V16
namespace Ocpp16 {
class Configuration;
class TransactionService;
class TransactionServiceEvse;
} //namespace Ocpp16
#endif //MO_ENABLE_V16

#if MO_ENABLE_V201
namespace Ocpp201 {
class Variable;
class TransactionService;
class TransactionServiceEvse;
} //namespace Ocpp201
#endif //MO_ENABLE_V201

class RemoteControlServiceEvse : public MemoryManaged {
private:
    Context& context;
    RemoteControlService& rcService;
    const unsigned int evseId;

    #if MO_ENABLE_V16
    Ocpp16::TransactionServiceEvse *txServiceEvse16 = nullptr;
    #endif
    #if MO_ENABLE_V201
    Ocpp201::TransactionServiceEvse *txServiceEvse201 = nullptr;
    #endif

#if MO_ENABLE_CONNECTOR_LOCK
    MO_UnlockConnectorResult (*onUnlockConnector)(unsigned int evseId, void *user) = nullptr;
    void *onUnlockConnectorUserData = nullptr;
#endif //MO_ENABLE_CONNECTOR_LOCK

public:
    RemoteControlServiceEvse(Context& context, RemoteControlService& rcService, unsigned int evseId);

#if MO_ENABLE_CONNECTOR_LOCK
    void setOnUnlockConnector(MO_UnlockConnectorResult (*onUnlockConnector)(unsigned int evseId, void *userData), void *userData);
#endif //MO_ENABLE_CONNECTOR_LOCK

    bool setup();

#if MO_ENABLE_CONNECTOR_LOCK
    #if MO_ENABLE_V16
    Ocpp16::UnlockStatus unlockConnector16();
    #endif
    #if MO_ENABLE_V201
    Ocpp201::UnlockStatus unlockConnector201();
    #endif
#endif //MO_ENABLE_CONNECTOR_LOCK
};

class RemoteControlService : public MemoryManaged {
private:
    Context& context;
    RemoteControlServiceEvse* evses [MO_NUM_EVSEID] = {nullptr};
    unsigned int numEvseId = MO_NUM_EVSEID;

    #if MO_ENABLE_V16
    Ocpp16::TransactionService *txService16 = nullptr;
    Ocpp16::Configuration *authorizeRemoteTxRequests = nullptr;
    #endif
    #if MO_ENABLE_V201
    Ocpp201::TransactionService *txService201 = nullptr;
    Ocpp201::Variable *authorizeRemoteStart = nullptr;
    #endif

    struct OperationCreator {
        const char *operationType = nullptr;
        Operation* (*create)(Context& context) = nullptr;
        Operation* (*create2)(Context& context, unsigned int evseId) = nullptr;
        TriggerMessageStatus (*handler3)(Context& context, int evseId) = nullptr;
    };
    Vector<OperationCreator> triggerMessageHandlers;

public:
    RemoteControlService(Context& context);
    ~RemoteControlService();

    RemoteControlServiceEvse *getEvse(unsigned int evseId);

    bool addTriggerMessageHandler(const char *operationType, Operation* (*createOperationCb)(Context& context));
    bool addTriggerMessageHandler(const char *operationType, Operation* (*createOperationCb)(Context& context, unsigned int evseId));
    bool addTriggerMessageHandler(const char *operationType, TriggerMessageStatus (*triggerMessageHandlerCb)(Context& context, int evseId));

    bool setup();

    #if MO_ENABLE_V16
    Ocpp16::RemoteStartStopStatus remoteStartTransaction(int connectorId, const char *idTag, std::unique_ptr<ChargingProfile> chargingProfile); 

    Ocpp16::RemoteStartStopStatus remoteStopTransaction(int transactionId);
    #endif //MO_ENABLE_V16

    #if MO_ENABLE_V201
    Ocpp201::RequestStartStopStatus requestStartTransaction(unsigned int evseId, unsigned int remoteStartId, Ocpp201::IdToken idToken, char *transactionIdOut, size_t transactionIdBufSize); //ChargingProfile, GroupIdToken not supported yet

    Ocpp201::RequestStartStopStatus requestStopTransaction(const char *transactionId);
    #endif //MO_ENABLE_V201

    TriggerMessageStatus triggerMessage(const char *requestedMessage, int evseId = -1);

friend RemoteControlServiceEvse;
};

} //namespace MicroOcpp
#endif //MO_ENABLE_V16 || MO_ENABLE_V201
#endif
