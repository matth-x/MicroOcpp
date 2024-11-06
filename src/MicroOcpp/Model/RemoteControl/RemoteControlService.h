// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_REMOTECONTROLSERVICE_H
#define MO_REMOTECONTROLSERVICE_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Model/RemoteControl/RemoteControlDefs.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Model/ConnectorBase/EvseId.h>
#include <MicroOcpp/Core/Memory.h>

namespace MicroOcpp {

class Context;
class Variable;

class RemoteControlServiceEvse : public MemoryManaged {
private:
    Context& context;
    const unsigned int evseId;

#if MO_ENABLE_CONNECTOR_LOCK
    UnlockConnectorResult (*onUnlockConnector)(unsigned int evseId, void *user) = nullptr;
    void *onUnlockConnectorUserData = nullptr;
#endif

public:
    RemoteControlServiceEvse(Context& context, unsigned int evseId);

#if MO_ENABLE_CONNECTOR_LOCK
    void setOnUnlockConnector(UnlockConnectorResult (*onUnlockConnector)(unsigned int evseId, void *userData), void *userData);

    UnlockStatus unlockConnector();
#endif

};

class RemoteControlService : public MemoryManaged {
private:
    Context& context;
    RemoteControlServiceEvse* evses [MO_NUM_EVSEID] = {nullptr};

    Variable *authorizeRemoteStart = nullptr;

public:
    RemoteControlService(Context& context, size_t numEvses);
    ~RemoteControlService();

    RemoteControlServiceEvse *getEvse(unsigned int evseId);

    RequestStartStopStatus requestStartTransaction(unsigned int evseId, unsigned int remoteStartId, IdToken idToken, char *transactionIdOut, size_t transactionIdBufSize); //ChargingProfile, GroupIdToken not supported yet

    RequestStartStopStatus requestStopTransaction(const char *transactionId);
};

} // namespace MicroOcpp

#endif // MO_ENABLE_V201

#endif
