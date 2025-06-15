// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/RemoteControl/RemoteControlService.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Debug.h>
#include <MicroOcpp/Model/Boot/BootService.h>
#include <MicroOcpp/Model/Availability/AvailabilityService.h>
#include <MicroOcpp/Model/Configuration/ConfigurationService.h>
#include <MicroOcpp/Model/Diagnostics/DiagnosticsService.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Model/SmartCharging/SmartChargingService.h>
#include <MicroOcpp/Model/Transactions/TransactionService16.h>
#include <MicroOcpp/Model/Transactions/TransactionService201.h>
#include <MicroOcpp/Operations/BootNotification.h>
#include <MicroOcpp/Operations/DiagnosticsStatusNotification.h>
#include <MicroOcpp/Operations/RemoteStartTransaction.h>
#include <MicroOcpp/Operations/RemoteStopTransaction.h>
#include <MicroOcpp/Operations/RequestStartTransaction.h>
#include <MicroOcpp/Operations/RequestStopTransaction.h>
#include <MicroOcpp/Operations/TriggerMessage.h>
#include <MicroOcpp/Operations/UnlockConnector.h>

#if MO_ENABLE_V16 || MO_ENABLE_V201

using namespace MicroOcpp;

RemoteControlServiceEvse::RemoteControlServiceEvse(Context& context, RemoteControlService& rcService, unsigned int evseId) :
        MemoryManaged("v16/v201.RemoteControl.RemoteControlServiceEvse"),
        context(context),
        rcService(rcService),
        evseId(evseId) {

}

bool RemoteControlServiceEvse::setup() {

    #if MO_ENABLE_V16
    if (context.getOcppVersion() == MO_OCPP_V16) {
        auto txServce = context.getModel16().getTransactionService();
        txServiceEvse16 = txServce ? txServce->getEvse(evseId) : nullptr;
        if (!txServiceEvse16) {
            MO_DBG_ERR("setup failure");
            return false;
        }
    }
    #endif
    #if MO_ENABLE_V201
    if (context.getOcppVersion() == MO_OCPP_V201) {
        auto txServce = context.getModel201().getTransactionService();
        txServiceEvse201 = txServce ? txServce->getEvse(evseId) : nullptr;
        if (!txServiceEvse201) {
            MO_DBG_ERR("setup failure");
            return false;
        }
    }
    #endif

    #if MO_ENABLE_CONNECTOR_LOCK
    if (!onUnlockConnector) {
        MO_DBG_WARN("need to set onUnlockConnector");
    }
    #endif

    return true;
}

#if MO_ENABLE_CONNECTOR_LOCK
void RemoteControlServiceEvse::setOnUnlockConnector(MO_UnlockConnectorResult (*onUnlockConnector)(unsigned int evseId, void *userData), void *userData) {
    this->onUnlockConnector = onUnlockConnector;
    this->onUnlockConnectorUserData = userData;
}

#if MO_ENABLE_V16
Ocpp16::UnlockStatus RemoteControlServiceEvse::unlockConnector16() {

    if (!onUnlockConnector) {
        return Ocpp16::UnlockStatus::UnlockFailed;
    }

    auto tx = txServiceEvse16->getTransaction();
    if (tx && tx->isActive()) {
        txServiceEvse16->endTransaction(nullptr, "UnlockCommand");
        txServiceEvse16->updateTxNotification(MO_TxNotification_RemoteStop);
    }

    auto status = onUnlockConnector(evseId, onUnlockConnectorUserData);
    switch (status) {
        case MO_UnlockConnectorResult_Pending:
            return Ocpp16::UnlockStatus::PENDING;
        case MO_UnlockConnectorResult_Unlocked:
            return Ocpp16::UnlockStatus::Unlocked;
        case MO_UnlockConnectorResult_UnlockFailed:
            return Ocpp16::UnlockStatus::UnlockFailed;
    }

    MO_DBG_ERR("invalid onUnlockConnector result code");
    return Ocpp16::UnlockStatus::UnlockFailed;
}
#endif //MO_ENABLE_V16

#if MO_ENABLE_V201
Ocpp201::UnlockStatus RemoteControlServiceEvse::unlockConnector201() {

    if (!onUnlockConnector) {
        return Ocpp201::UnlockStatus::UnlockFailed;
    }

    if (auto tx = txServiceEvse201->getTransaction()) {
        if (tx->started && !tx->stopped && tx->isAuthorized) {
            return Ocpp201::UnlockStatus::OngoingAuthorizedTransaction;
        } else {
            txServiceEvse201->abortTransaction(MO_TxStoppedReason_Other,MO_TxEventTriggerReason_UnlockCommand);
        }
    }

    auto status = onUnlockConnector(evseId, onUnlockConnectorUserData);
    switch (status) {
        case MO_UnlockConnectorResult_Pending:
            return Ocpp201::UnlockStatus::PENDING;
        case MO_UnlockConnectorResult_Unlocked:
            return Ocpp201::UnlockStatus::Unlocked;
        case MO_UnlockConnectorResult_UnlockFailed:
            return Ocpp201::UnlockStatus::UnlockFailed;
    }

    MO_DBG_ERR("invalid onUnlockConnector result code");
    return Ocpp201::UnlockStatus::UnlockFailed;
}
#endif //MO_ENABLE_V201

#endif //MO_ENABLE_CONNECTOR_LOCK

RemoteControlService::RemoteControlService(Context& context) : MemoryManaged("v16/v201.RemoteControl.RemoteControlService"), context(context) {

}

RemoteControlService::~RemoteControlService() {
    for (unsigned int i = 0; i < MO_NUM_EVSEID; i++) {
        delete evses[i];
        evses[i] = nullptr;
    }
}

RemoteControlServiceEvse *RemoteControlService::getEvse(unsigned int evseId) {
    if (evseId >= numEvseId) {
        MO_DBG_ERR("evseId out of bound");
        return nullptr;
    }

    if (!evses[evseId]) {
        evses[evseId] = new RemoteControlServiceEvse(context, *this, evseId);
        if (!evses[evseId]) {
            MO_DBG_ERR("OOM");
            return nullptr;
        }
    }

    return evses[evseId];
}

bool RemoteControlService::setup() {

    unsigned int numEvseId = MO_NUM_EVSEID;

    #if MO_ENABLE_V16
    if (context.getOcppVersion() == MO_OCPP_V16) {

        auto configService = context.getModel16().getConfigurationService();
        if (!configService) {
            MO_DBG_ERR("setup failure");
            return false;
        }
    
        configService->declareConfiguration<bool>("AuthorizeRemoteTxRequests", false);
    
    #if MO_ENABLE_CONNECTOR_LOCK
        configService->declareConfiguration<bool>("UnlockConnectorOnEVSideDisconnect", true); //read-write
    #else
        configService->declareConfiguration<bool>("UnlockConnectorOnEVSideDisconnect", false, MO_CONFIGURATION_VOLATILE, Mutability::ReadOnly); //read-only because there is no connector lock
    #endif //MO_ENABLE_CONNECTOR_LOCK
    
        txService16 = context.getModel16().getTransactionService();
        if (!txService16) {
            MO_DBG_ERR("setup failure");
            return false;
        }

        numEvseId = context.getModel16().getNumEvseId();

        context.getMessageService().registerOperation("RemoteStartTransaction", [] (Context& context) -> Operation* {
            return new Ocpp16::RemoteStartTransaction(context, *context.getModel16().getRemoteControlService());});
        context.getMessageService().registerOperation("RemoteStopTransaction", [] (Context& context) -> Operation* {
            return new Ocpp16::RemoteStopTransaction(context, *context.getModel16().getRemoteControlService());});
        context.getMessageService().registerOperation("UnlockConnector", [] (Context& context) -> Operation* {
            return new Ocpp16::UnlockConnector(context, *context.getModel16().getRemoteControlService());});
    }
    #endif
    #if MO_ENABLE_V201
    if (context.getOcppVersion() == MO_OCPP_V201) {

        auto varService = context.getModel201().getVariableService();
        if (!varService) {
            return false;
        }

        authorizeRemoteStart = varService->declareVariable<bool>("AuthCtrlr", "AuthorizeRemoteStart", false);
        if (!authorizeRemoteStart) {
            MO_DBG_ERR("failure to declare variable");
            return false;
        }

        txService201 = context.getModel201().getTransactionService();
        if (!txService201) {
            MO_DBG_ERR("setup failure");
            return false;
        }

        numEvseId = context.getModel201().getNumEvseId();

        context.getMessageService().registerOperation("RequestStartTransaction", [] (Context& context) -> Operation* {
            return new Ocpp201::RequestStartTransaction(*context.getModel201().getRemoteControlService());});
        context.getMessageService().registerOperation("RequestStopTransaction", [] (Context& context) -> Operation* {
            return new Ocpp201::RequestStopTransaction(*context.getModel201().getRemoteControlService());});

        #if MO_ENABLE_CONNECTOR_LOCK
        context.getMessageService().registerOperation("UnlockConnector", [] (Context& context) -> Operation* {
            return new Ocpp201::UnlockConnector(context, *context.getModel201().getRemoteControlService());});
        #endif
    }
    #endif

    context.getMessageService().registerOperation("TriggerMessage", [] (Context& context) -> Operation* {
        RemoteControlService *rcSvc = nullptr;
        #if MO_ENABLE_V16
        if (context.getOcppVersion() == MO_OCPP_V16) {
            rcSvc = context.getModel16().getRemoteControlService();
        }
        #endif //MO_ENABLE_V16
        #if MO_ENABLE_V201
        if (context.getOcppVersion() == MO_OCPP_V201) {
            rcSvc = context.getModel201().getRemoteControlService();
        }
        #endif //MO_ENABLE_V201
        return new TriggerMessage(context, *rcSvc);});

    for (unsigned int i = 0; i < numEvseId; i++) {
        if (!getEvse(i) || !getEvse(i)->setup()) {
            MO_DBG_ERR("setup failure");
            return false;
        }
    }

    return true;
}

bool RemoteControlService::addTriggerMessageHandler(const char *operationType, Operation* (*createOperationCb)(Context& context)) {
    bool capacity = triggerMessageHandlers.size() + 1;
    triggerMessageHandlers.reserve(capacity);
    if (triggerMessageHandlers.capacity() < capacity) {
        MO_DBG_ERR("OOM");
        return false;
    }
    OperationCreator handler;
    handler.operationType = operationType;
    handler.create = createOperationCb;
    triggerMessageHandlers.push_back(handler);
    return true;
}

bool RemoteControlService::addTriggerMessageHandler(const char *operationType, Operation* (*createOperationCb)(Context& context, unsigned int evseId)) {
    bool capacity = triggerMessageHandlers.size() + 1;
    triggerMessageHandlers.reserve(capacity);
    if (triggerMessageHandlers.capacity() < capacity) {
        MO_DBG_ERR("OOM");
        return false;
    }
    OperationCreator handler;
    handler.operationType = operationType;
    handler.create2 = createOperationCb;
    triggerMessageHandlers.push_back(handler);
    return true;
}

bool RemoteControlService::addTriggerMessageHandler(const char *operationType, TriggerMessageStatus (*triggerMessageHandlerCb)(Context& context, int evseId)) {
    bool capacity = triggerMessageHandlers.size() + 1;
    triggerMessageHandlers.reserve(capacity);
    if (triggerMessageHandlers.capacity() < capacity) {
        MO_DBG_ERR("OOM");
        return false;
    }
    OperationCreator handler;
    handler.operationType = operationType;
    handler.handler3 = triggerMessageHandlerCb;
    triggerMessageHandlers.push_back(handler);
    return true;
}

#if MO_ENABLE_V16
Ocpp16::RemoteStartStopStatus RemoteControlService::remoteStartTransaction(int connectorId, const char *idTag, std::unique_ptr<ChargingProfile> chargingProfile) {

    auto configService = context.getModel16().getConfigurationService();
    auto authorizeRemoteTxRequests = configService ? configService->declareConfiguration<bool>("AuthorizeRemoteTxRequests", false) : nullptr;
    if (!authorizeRemoteTxRequests) {
        MO_DBG_ERR("internal error");
        return Ocpp16::RemoteStartStopStatus::ERR_INTERNAL;
    }

    auto status = Ocpp16::RemoteStartStopStatus::Rejected;

    Ocpp16::TransactionServiceEvse *selectEvse = nullptr;
    if (connectorId >= 1) {
        //connectorId specified for given connector, try to start Transaction here
        auto txSvcEvse = txService16->getEvse(connectorId);
        auto availSvc = context.getModel16().getAvailabilityService();
        auto availSvcEvse = availSvc ? availSvc->getEvse(connectorId) : nullptr;
        if (txSvcEvse && availSvcEvse){
            if (!txSvcEvse->getTransaction() &&
                    availSvcEvse->isOperative()) {
                selectEvse = txSvcEvse;
            }
        }
    } else {
        //connectorId not specified. Find free connector
        for (unsigned int eId = 1; eId < numEvseId; eId++) {
            auto txSvcEvse = txService16->getEvse(eId);
            auto availSvc = context.getModel16().getAvailabilityService();
            auto availSvcEvse = availSvc ? availSvc->getEvse(eId) : nullptr;
            if (txSvcEvse && availSvcEvse) {
                if (!txSvcEvse->getTransaction() &&
                        availSvcEvse->isOperative()) {
                    selectEvse = txSvcEvse;
                    connectorId = eId;
                    break;
                }
            }
        }
    }

    if (selectEvse) {

        bool success = true;

        int chargingProfileId = -1; //keep Id after moving charging profile to SCService

        if (chargingProfile && context.getModel16().getSmartChargingService()) {
            chargingProfileId = chargingProfile->chargingProfileId;
            success = context.getModel16().getSmartChargingService()->setChargingProfile(connectorId, std::move(chargingProfile));
        }

        if (success) {
            if (authorizeRemoteTxRequests->getBool()) {
                success = selectEvse->beginTransaction(idTag);
            } else {
                success = selectEvse->beginTransaction_authorized(idTag);
            }
        }
        if (success) {
            if (auto transaction = selectEvse->getTransaction()) {
                selectEvse->updateTxNotification(MO_TxNotification_RemoteStart);
    
                if (chargingProfileId >= 0) {
                    transaction->setTxProfileId(chargingProfileId);
                }
            } else {
                MO_DBG_ERR("internal error");
                status = Ocpp16::RemoteStartStopStatus::ERR_INTERNAL;
                success = false;
            }
        }

        if (!success && chargingProfile && context.getModel16().getSmartChargingService()) {
            context.getModel16().getSmartChargingService()->clearChargingProfile(chargingProfile->chargingProfileId, connectorId, ChargingProfilePurposeType::UNDEFINED, -1);
        }

        if (success) {
            status = Ocpp16::RemoteStartStopStatus::Accepted;
        } else {
            status = Ocpp16::RemoteStartStopStatus::Rejected;
        }
    } else {
        MO_DBG_INFO("No connector to start transaction");
        status = Ocpp16::RemoteStartStopStatus::Rejected;
    }

    return status;
}

Ocpp16::RemoteStartStopStatus RemoteControlService::remoteStopTransaction(int transactionId) {

    auto status = Ocpp16::RemoteStartStopStatus::Rejected;

    for (unsigned int eId = 0; eId < numEvseId; eId++) {
        auto txSvcEvse = txService16->getEvse(eId);
        auto transaction = txSvcEvse ? txSvcEvse->getTransaction() : nullptr;
        if (transaction &&
                txSvcEvse->getTransaction()->getTransactionId() == transactionId) {
            if (transaction->isActive()) {
                txSvcEvse->updateTxNotification(MO_TxNotification_RemoteStop);
            }
            txSvcEvse->endTransaction(nullptr, "Remote");
            status = Ocpp16::RemoteStartStopStatus::Accepted;
        }
    }

    return status;
}
#endif //MO_ENABLE_V16

#if MO_ENABLE_V201

Ocpp201::RequestStartStopStatus RemoteControlService::requestStartTransaction(unsigned int evseId, unsigned int remoteStartId, Ocpp201::IdToken idToken, char *transactionIdOut, size_t transactionIdBufSize) {
    
    if (!txService201) {
        MO_DBG_ERR("TxService uninitialized");
        return Ocpp201::RequestStartStopStatus::Rejected;
    }

    auto evse = txService201->getEvse(evseId);
    if (!evse) {
        MO_DBG_ERR("EVSE not found");
        return Ocpp201::RequestStartStopStatus::Rejected;
    }

    if (!evse->beginAuthorization(idToken, authorizeRemoteStart->getBool())) {
        MO_DBG_INFO("EVSE still occupied with pending tx");
        if (auto tx = evse->getTransaction()) {
            auto ret = snprintf(transactionIdOut, transactionIdBufSize, "%s", tx->transactionId);
            if (ret < 0 || (size_t)ret >= transactionIdBufSize) {
                MO_DBG_ERR("internal error");
                return Ocpp201::RequestStartStopStatus::Rejected;
            }
        }
        return Ocpp201::RequestStartStopStatus::Rejected;
    }

    auto tx = evse->getTransaction();
    if (!tx) {
        MO_DBG_ERR("internal error");
        return Ocpp201::RequestStartStopStatus::Rejected;
    }

    auto ret = snprintf(transactionIdOut, transactionIdBufSize, "%s", tx->transactionId);
    if (ret < 0 || (size_t)ret >= transactionIdBufSize) {
        MO_DBG_ERR("internal error");
        return Ocpp201::RequestStartStopStatus::Rejected;
    }

    tx->remoteStartId = remoteStartId;
    tx->notifyRemoteStartId = true;

    return Ocpp201::RequestStartStopStatus::Accepted;
}

Ocpp201::RequestStartStopStatus RemoteControlService::requestStopTransaction(const char *transactionId) {

    if (!txService201) {
        MO_DBG_ERR("TxService uninitialized");
        return Ocpp201::RequestStartStopStatus::Rejected;
    }

    bool success = false;

    for (unsigned int evseId = 0; evseId < MO_NUM_EVSEID; evseId++) {
        if (auto evse = txService201->getEvse(evseId)) {
            if (evse->getTransaction() && !strcmp(evse->getTransaction()->transactionId, transactionId)) {
                success = evse->abortTransaction(MO_TxStoppedReason_Remote, MO_TxEventTriggerReason_RemoteStop);
                break;
            }
        }
    }

    return success ?
            Ocpp201::RequestStartStopStatus::Accepted :
            Ocpp201::RequestStartStopStatus::Rejected;
}

#endif //MO_ENABLE_V201

TriggerMessageStatus RemoteControlService::triggerMessage(const char *requestedMessage, int evseId) {

    TriggerMessageStatus status = TriggerMessageStatus::NotImplemented;

    for (size_t i = 0; i < triggerMessageHandlers.size(); i++) {

        auto& handler = triggerMessageHandlers[i];

        if (!strcmp(requestedMessage, handler.operationType)) {

            if (handler.handler3) {
                // handler3 is a fully custom implementation. Just execute the handler with the parameters
                // from TriggerMessage and we're done
                status = handler.handler3(context, evseId);
                break;
            }

            unsigned int evseIdRangeBegin = 0, evseIdRangeEnd = 1;
            if (handler.create2) {
                // create2 is defined if multiple evseIds are supported. Then determine evseId range
                if (evseId < 0) {
                    evseIdRangeBegin = 0;
                    evseIdRangeEnd = numEvseId;
                } else if ((unsigned int)evseId < numEvseId) {
                    evseIdRangeBegin = (unsigned int)evseId;
                    evseIdRangeEnd = (unsigned int)evseId + 1;
                } else {
                    return TriggerMessageStatus::ERR_INTERNAL;
                }
            }

            for (unsigned int eId = evseIdRangeBegin; eId < evseIdRangeEnd; eId++) {

                Operation *operation = nullptr;

                if (handler.create) {
                    operation = handler.create(context);
                } else if (handler.create2) {
                    operation = handler.create2(context, eId);
                }

                if (!operation) {
                    MO_DBG_ERR("OOM");
                    return TriggerMessageStatus::ERR_INTERNAL;
                }

                auto request = makeRequest(context, operation);
                if (!request) {
                    MO_DBG_ERR("OOM");
                    return TriggerMessageStatus::ERR_INTERNAL;
                }

                bool success = context.getMessageService().sendRequest(std::move(request));
                if (success) {
                    status = TriggerMessageStatus::Accepted;
                } else {
                    MO_DBG_ERR("OOM");
                    return TriggerMessageStatus::ERR_INTERNAL;
                }
            }

            // handler found, done
            break;
        }
    }

    return status;
}

#endif //MO_ENABLE_V16 || MO_ENABLE_V201
