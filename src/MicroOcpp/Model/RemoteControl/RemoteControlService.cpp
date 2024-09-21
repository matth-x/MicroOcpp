// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Model/RemoteControl/RemoteControlService.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Debug.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Model/Transactions/TransactionService.h>
#include <MicroOcpp/Operations/RequestStartTransaction.h>
#include <MicroOcpp/Operations/RequestStopTransaction.h>
#include <MicroOcpp/Operations/TriggerMessage.h>
#include <MicroOcpp/Operations/UnlockConnector.h>

using namespace MicroOcpp;

RemoteControlServiceEvse::RemoteControlServiceEvse(Context& context, unsigned int evseId) : MemoryManaged("v201.RemoteControl.RemoteControlServiceEvse"), context(context), evseId(evseId) {

}

#if MO_ENABLE_CONNECTOR_LOCK
void RemoteControlServiceEvse::setOnUnlockConnector(UnlockConnectorResult (*onUnlockConnector)(unsigned int evseId, void *userData), void *userData) {
    this->onUnlockConnector = onUnlockConnector;
    this->onUnlockConnectorUserData = userData;
}

UnlockStatus RemoteControlServiceEvse::unlockConnector() {

    if (!onUnlockConnector) {
        return UnlockStatus_UnlockFailed;
    }

    if (auto txService = context.getModel().getTransactionService()) {
        if (auto evse = txService->getEvse(evseId)) {
            if (auto tx = evse->getTransaction()) {
                if (tx->started && !tx->stopped && tx->isAuthorized) {
                    return UnlockStatus_OngoingAuthorizedTransaction;
                } else {
                    evse->abortTransaction(Ocpp201::Transaction::StopReason::Other,Ocpp201::TransactionEventTriggerReason::UnlockCommand);
                }
            }
        }
    }

    auto status = onUnlockConnector(evseId, onUnlockConnectorUserData);
    switch (status) {
        case UnlockConnectorResult_Pending:
            return UnlockStatus_PENDING;
        case UnlockConnectorResult_Unlocked:
            return UnlockStatus_Unlocked;
        case UnlockConnectorResult_UnlockFailed:
            return UnlockStatus_UnlockFailed;
    }

    MO_DBG_ERR("invalid onUnlockConnector result code");
    return UnlockStatus_UnlockFailed;
}
#endif

RemoteControlService::RemoteControlService(Context& context, size_t numEvses) : MemoryManaged("v201.RemoteControl.RemoteControlService"), context(context) {

    for (size_t i = 0; i < numEvses && i < MO_NUM_EVSE; i++) {
        evses[i] = new RemoteControlServiceEvse(context, (unsigned int)i);
    }

    auto varService = context.getModel().getVariableService();
    authorizeRemoteStart = varService->declareVariable<bool>("AuthCtrlr", "AuthorizeRemoteStart", false);

    context.getOperationRegistry().registerOperation("RequestStartTransaction", [this] () -> Operation* {
        if (!this->context.getModel().getTransactionService()) {
            return nullptr; //-> NotSupported
        }
        return new Ocpp201::RequestStartTransaction(*this);});
    context.getOperationRegistry().registerOperation("RequestStopTransaction", [this] () -> Operation* {
        if (!this->context.getModel().getTransactionService()) {
            return nullptr; //-> NotSupported
        }
        return new Ocpp201::RequestStopTransaction(*this);});
#if MO_ENABLE_CONNECTOR_LOCK
    context.getOperationRegistry().registerOperation("UnlockConnector", [this] () {
        return new Ocpp201::UnlockConnector(*this);});
#endif
    context.getOperationRegistry().registerOperation("TriggerMessage", [&context] () {
        return new Ocpp16::TriggerMessage(context);});
}

RemoteControlService::~RemoteControlService() {
    for (size_t i = 0; i < MO_NUM_EVSE && evses[i]; i++) {
        delete evses[i];
    }
}

RemoteControlServiceEvse *RemoteControlService::getEvse(unsigned int evseId) {
    if (evseId >= MO_NUM_EVSE) {
        MO_DBG_ERR("invalid arg");
        return nullptr;
    }
    return evses[evseId];
}

RequestStartStopStatus RemoteControlService::requestStartTransaction(unsigned int evseId, unsigned int remoteStartId, IdToken idToken, std::shared_ptr<Ocpp201::Transaction>& transactionOut) {
    
    TransactionService *txService = context.getModel().getTransactionService();
    if (!txService) {
        MO_DBG_ERR("TxService uninitialized");
        return RequestStartStopStatus_Rejected;
    }
    
    auto evse = txService->getEvse(evseId);
    if (!evse) {
        MO_DBG_ERR("EVSE not found");
        return RequestStartStopStatus_Rejected;
    }

    transactionOut = evse->getTransaction();

    if (!evse->beginAuthorization(idToken, authorizeRemoteStart->getBool())) {
        MO_DBG_INFO("EVSE still occupied with pending tx");
        return RequestStartStopStatus_Rejected;
    }

    transactionOut = evse->getTransaction();
    if (!transactionOut) {
        MO_DBG_ERR("internal error");
        return RequestStartStopStatus_Rejected;
    }

    transactionOut->remoteStartId = remoteStartId;
    transactionOut->notifyRemoteStartId = true;

    return RequestStartStopStatus_Accepted;
}

RequestStartStopStatus RemoteControlService::requestStopTransaction(const char *transactionId) {

    TransactionService *txService = context.getModel().getTransactionService();
    if (!txService) {
        MO_DBG_ERR("TxService uninitialized");
        return RequestStartStopStatus_Rejected;
    }

    bool success = false;

    for (unsigned int evseId = 0; evseId < MO_NUM_EVSE; evseId++) {
        if (auto evse = txService->getEvse(evseId)) {
            if (evse->getTransaction() && !strcmp(evse->getTransaction()->transactionId, transactionId)) {
                success = evse->abortTransaction(Ocpp201::Transaction::StopReason::Remote, Ocpp201::TransactionEventTriggerReason::RemoteStop);
                break;
            }
        }
    }

    return success ?
            RequestStartStopStatus_Accepted :
            RequestStartStopStatus_Rejected;
}

#endif // MO_ENABLE_V201
