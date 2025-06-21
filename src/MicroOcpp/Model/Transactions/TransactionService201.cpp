// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Transactions/TransactionService201.h>

#include <string.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlService.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Operations/Authorize.h>
#include <MicroOcpp/Operations/TransactionEvent.h>
#include <MicroOcpp/Operations/RequestStartTransaction.h>
#include <MicroOcpp/Operations/RequestStopTransaction.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V201

#ifndef MO_TX_CLEAN_ABORTED
#define MO_TX_CLEAN_ABORTED 1
#endif

using namespace MicroOcpp;
using namespace MicroOcpp::Ocpp201;

TransactionServiceEvse::TransactionServiceEvse(Context& context, TransactionService& txService, TransactionStoreEvse& txStore, unsigned int evseId) :
        MemoryManaged("v201.Transactions.TransactionServiceEvse"),
        context(context),
        clock(context.getClock()),
        txService(txService),
        txStore(txStore),
        evseId(evseId) {

}

TransactionServiceEvse::~TransactionServiceEvse() {

}

bool TransactionServiceEvse::setup() {
    context.getMessageService().addSendQueue(this); //register at RequestQueue as Request emitter

    auto meteringService = context.getModel201().getMeteringService();
    meteringEvse = meteringService ? meteringService->getEvse(evseId) : nullptr;
    if (!meteringEvse) {
        MO_DBG_ERR("depends on MeteringService");
        return false;
    }

    if (txService.filesystem) {

        char fnPrefix [MO_MAX_PATH_SIZE];
        snprintf(fnPrefix, sizeof(fnPrefix), "tx201-%u-", evseId);

        if (!FilesystemUtils::loadRingIndex(txService.filesystem, fnPrefix, MO_TXNR_MAX, &txNrBegin, &txNrEnd)) {
            MO_DBG_ERR("failure to init tx index");
            return false;
        }

        txNrFront = txNrBegin;
        MO_DBG_DEBUG("found %u transactions for evseId %u. Internal range from %u to %u (exclusive)", (txNrEnd + MO_TXNR_MAX - txNrBegin) % MO_TXNR_MAX, evseId, txNrBegin, txNrEnd);

        unsigned int txNrLatest = (txNrEnd + MO_TXNR_MAX - 1) % MO_TXNR_MAX; //txNr of the most recent tx on flash
        transaction = txStore.loadTransaction(txNrLatest); //returns nullptr if txNrLatest does not exist on flash
    }

    return true;
}

bool TransactionServiceEvse::beginTransaction() {

    if (transaction) {
        MO_DBG_ERR("transaction still running");
        return false;
    }

    std::unique_ptr<Transaction> tx;

    char txId [sizeof(Transaction::transactionId)];

    if (!UuidUtils::generateUUID(context.getRngCb(), txId, sizeof(txId))) {
        MO_DBG_ERR("UUID failure");
        return false;
    }

    //clean possible aborted tx
    unsigned int txr = txNrEnd;
    unsigned int txSize = (txNrEnd + MO_TXNR_MAX - txNrBegin) % MO_TXNR_MAX;
    for (unsigned int i = 0; i < txSize; i++) {
        txr = (txr + MO_TXNR_MAX - 1) % MO_TXNR_MAX; //decrement by 1

        std::unique_ptr<Transaction> intermediateTx;

        Transaction *txhist = nullptr;
        if (transaction && transaction->txNr == txr) {
            txhist = transaction.get();
        } else if (txFront && txFront->txNr == txr) {
            txhist = txFront;
        } else {
            intermediateTx = txStore.loadTransaction(txr);
            txhist = intermediateTx.get();
        }

        //check if dangling silent tx, aborted tx, or corrupted entry (txhist == null)
        if (!txhist || txhist->silent || (!txhist->active && !txhist->started && MO_TX_CLEAN_ABORTED)) {
            //yes, remove
            if (txStore.remove(txr)) {
                if (txNrFront == txNrEnd) {
                    txNrFront = txr;
                }
                txNrEnd = txr;
                MO_DBG_WARN("deleted dangling silent or aborted tx for new transaction");
            } else {
                MO_DBG_ERR("memory corruption");
                break;
            }
        } else {
            //no, tx record trimmed, end
            break;
        }
    }

    txSize = (txNrEnd + MO_TXNR_MAX - txNrBegin) % MO_TXNR_MAX; //refresh after cleaning txs

    //try to create new transaction
    if (txSize < MO_TXRECORD_SIZE_V201) {
        tx = txStore.createTransaction(txNrEnd, txId);
    }

    if (!tx) {
        //could not create transaction - now, try to replace tx history entry

        unsigned int txl = txNrBegin;
        txSize = (txNrEnd + MO_TXNR_MAX - txNrBegin) % MO_TXNR_MAX;

        for (unsigned int i = 0; i < txSize; i++) {

            if (tx) {
                //success, finished here
                break;
            }

            //no transaction allocated, delete history entry to make space
            std::unique_ptr<Transaction> intermediateTx;

            Transaction *txhist = nullptr;
            if (transaction && transaction->txNr == txl) {
                txhist = transaction.get();
            } else if (txFront && txFront->txNr == txl) {
                txhist = txFront;
            } else {
                intermediateTx = txStore.loadTransaction(txl);
                txhist = intermediateTx.get();
            }

            //oldest entry, now check if it's history and can be removed or corrupted entry
            if (!txhist || (txhist->stopped && txhist->seqNos.empty()) || (!txhist->active && !txhist->started) || (txhist->silent && txhist->stopped)) {
                //yes, remove

                if (txStore.remove(txl)) {
                    txNrBegin = (txl + 1) % MO_TXNR_MAX;
                    if (txNrFront == txl) {
                        txNrFront = txNrBegin;
                    }
                    MO_DBG_DEBUG("deleted tx history entry for new transaction");
                    MO_DBG_VERBOSE("txNrBegin=%u, txNrFront=%u, txNrEnd=%u", txNrBegin, txNrFront, txNrEnd);

                    tx = txStore.createTransaction(txNrEnd, txId);
                } else {
                    MO_DBG_ERR("memory corruption");
                    break;
                }
            } else {
                //no, end of history reached, don't delete further tx
                MO_DBG_DEBUG("cannot delete more tx");
                break;
            }

            txl++;
            txl %= MO_TXNR_MAX;
        }
    }

    if (!tx) {
        //couldn't create normal transaction -> check if to start charging without real transaction
        if (txService.silentOfflineTransactionsBool->getBool()) {
            //try to handle charging session without sending StartTx or StopTx to the server
            tx = txStore.createTransaction(txNrEnd, txId);

            if (tx) {
                tx->silent = true;
                MO_DBG_DEBUG("created silent transaction");
            }
        }
    }

    if (!tx) {
        MO_DBG_ERR("transaction queue full");
        return false;
    }

    tx->beginTimestamp = clock.now();

    if (!txStore.commit(tx.get())) {
        MO_DBG_ERR("fs error");
        return false;
    }

    transaction = std::move(tx);

    txNrEnd = (txNrEnd + 1) % MO_TXNR_MAX;
    MO_DBG_DEBUG("advance txNrEnd %u-%u", evseId, txNrEnd);
    MO_DBG_VERBOSE("txNrBegin=%u, txNrFront=%u, txNrEnd=%u", txNrBegin, txNrFront, txNrEnd);

    return true;
}

bool TransactionServiceEvse::endTransaction(MO_TxStoppedReason stoppedReason = MO_TxStoppedReason_Other, MO_TxEventTriggerReason stopTrigger = MO_TxEventTriggerReason_AbnormalCondition) {

    if (!transaction || !transaction->active) {
        //transaction already ended / not active anymore
        return false;
    }

    MO_DBG_DEBUG("End transaction started by idTag %s",
                            transaction->idToken.get());

    transaction->active = false;
    transaction->stopTrigger = stopTrigger;
    transaction->stoppedReason = stoppedReason;
    txStore.commit(transaction.get());

    return true;
}

TransactionEventData::ChargingState TransactionServiceEvse::getChargingState() {
    auto res = TransactionEventData::ChargingState::Idle;
    if (connectorPluggedInput && !connectorPluggedInput(evseId, connectorPluggedInputUserData)) {
        res = TransactionEventData::ChargingState::Idle;
    } else if (!transaction || !transaction->isAuthorizationActive || !transaction->isAuthorized) {
        res = TransactionEventData::ChargingState::EVConnected;
    } else if (evseReadyInput && !evseReadyInput(evseId, evseReadyInputUserData)) { 
        res = TransactionEventData::ChargingState::SuspendedEVSE;
    } else if (evReadyInput && !evReadyInput(evseId, evReadyInputUserData)) {
        res = TransactionEventData::ChargingState::SuspendedEV;
    } else if (ocppPermitsCharge()) {
        res = TransactionEventData::ChargingState::Charging;
    }
    return res;
}

void TransactionServiceEvse::loop() {

    if (transaction && !transaction->active && !transaction->started) {
        MO_DBG_DEBUG("collect aborted transaction %u-%s", evseId, transaction->transactionId);
        if (txFront == transaction.get()) {
            MO_DBG_DEBUG("pass ownership from tx to txFront");
            txFrontCache = std::move(transaction);
        }
        transaction = nullptr;
    }

    if (transaction && transaction->stopped) {
        MO_DBG_DEBUG("collect obsolete transaction %u-%s", evseId, transaction->transactionId);
        if (txFront == transaction.get()) {
            MO_DBG_DEBUG("pass ownership from tx to txFront");
            txFrontCache = std::move(transaction);
        }
        transaction = nullptr;
    }

    // tx-related behavior
    if (transaction) {
        if (connectorPluggedInput) {
            if (connectorPluggedInput(evseId, connectorPluggedInputUserData)) {
                // if cable has been plugged at least once, EVConnectionTimeout will never get triggered
                transaction->evConnectionTimeoutListen = false;
            }

            int32_t dtTxBegin;
            if (!clock.delta(clock.now(), transaction->beginTimestamp, dtTxBegin)) {
                dtTxBegin = txService.evConnectionTimeOutInt->getInt();
            }

            if (transaction->active &&
                    transaction->evConnectionTimeoutListen &&
                    transaction->beginTimestamp.isDefined() &&
                    txService.evConnectionTimeOutInt->getInt() > 0 &&
                    !connectorPluggedInput(evseId, connectorPluggedInputUserData) &&
                    dtTxBegin >= txService.evConnectionTimeOutInt->getInt()) {

                MO_DBG_INFO("Session mngt: timeout");
                endTransaction(MO_TxStoppedReason_Timeout, MO_TxEventTriggerReason_EVConnectTimeout);

                updateTxNotification(MO_TxNotification_ConnectionTimeout);
            }

            if (transaction->active &&
                    transaction->isDeauthorized &&
                    !transaction->started &&
                    (txService.isTxStartPoint(TxStartStopPoint::Authorized) || txService.isTxStartPoint(TxStartStopPoint::PowerPathClosed) ||
                     txService.isTxStopPoint(TxStartStopPoint::Authorized)  || txService.isTxStopPoint(TxStartStopPoint::PowerPathClosed))) {
                
                MO_DBG_INFO("Session mngt: Deauthorized before start");
                endTransaction(MO_TxStoppedReason_DeAuthorized, MO_TxEventTriggerReason_Deauthorized);
            }
        }
    }

    std::unique_ptr<TransactionEventData> txEvent;

    bool txStopCondition = false;

    {
        // stop tx?

        MO_TxEventTriggerReason triggerReason = MO_TxEventTriggerReason_UNDEFINED;
        MO_TxStoppedReason stoppedReason = MO_TxStoppedReason_UNDEFINED;

        if (transaction && !transaction->active) {
            // tx ended via endTransaction
            txStopCondition = true;
            triggerReason = transaction->stopTrigger;
            stoppedReason = transaction->stoppedReason;
        } else if ((txService.isTxStopPoint(TxStartStopPoint::EVConnected) ||
                    txService.isTxStopPoint(TxStartStopPoint::PowerPathClosed)) &&
                    connectorPluggedInput && !connectorPluggedInput(evseId, connectorPluggedInputUserData) &&
                    (txService.stopTxOnEVSideDisconnectBool->getBool() || !transaction || !transaction->started)) {
            txStopCondition = true;
            triggerReason = MO_TxEventTriggerReason_EVCommunicationLost;
            stoppedReason = MO_TxStoppedReason_EVDisconnected;
        } else if ((txService.isTxStopPoint(TxStartStopPoint::Authorized) ||
                    txService.isTxStopPoint(TxStartStopPoint::PowerPathClosed)) &&
                    (!transaction || !transaction->isAuthorizationActive)) {
            // user revoked authorization (or EV or any "local" entity)
            txStopCondition = true;
            triggerReason = MO_TxEventTriggerReason_StopAuthorized;
            stoppedReason = MO_TxStoppedReason_Local;
        } else if (txService.isTxStopPoint(TxStartStopPoint::EnergyTransfer) &&
                    evReadyInput && !evReadyInput(evseId, evReadyInputUserData)) {
            txStopCondition = true;
            triggerReason = MO_TxEventTriggerReason_ChargingStateChanged;
            stoppedReason = MO_TxStoppedReason_StoppedByEV;
        } else if (txService.isTxStopPoint(TxStartStopPoint::EnergyTransfer) &&
                    (evReadyInput || evseReadyInput) && // at least one of the two defined
                    !(evReadyInput && evReadyInput(evseId, evReadyInputUserData)) &&
                    !(evseReadyInput && evseReadyInput(evseId, evseReadyInputUserData))) {
            txStopCondition = true;
            triggerReason = MO_TxEventTriggerReason_ChargingStateChanged;
            stoppedReason = MO_TxStoppedReason_Other;
        } else if (txService.isTxStopPoint(TxStartStopPoint::Authorized) &&
                    transaction && transaction->isDeauthorized &&
                    txService.stopTxOnInvalidIdBool->getBool()) {
            // OCPP server rejected authorization
            txStopCondition = true;
            triggerReason = MO_TxEventTriggerReason_Deauthorized;
            stoppedReason = MO_TxStoppedReason_DeAuthorized;
        }

        if (txStopCondition &&
                transaction && transaction->started && transaction->active) {

            MO_DBG_INFO("Session mngt: TxStopPoint reached");
            endTransaction(stoppedReason, triggerReason);
        }

        if (transaction &&
                transaction->started && !transaction->stopped && !transaction->active &&
                (!stopTxReadyInput || stopTxReadyInput(evseId, stopTxReadyInputUserData))) {
            // yes, stop running tx

            txEvent = txStore.createTransactionEvent(*transaction);
            if (!txEvent) {
                // OOM
                return;
            }

            transaction->stopTrigger = triggerReason;
            transaction->stoppedReason = stoppedReason;

            txEvent->eventType = TransactionEventData::Type::Ended;
            txEvent->triggerReason = triggerReason;
        }
    } 
    
    if (!txStopCondition) {
        // start tx?

        bool txStartCondition = false;

        MO_TxEventTriggerReason triggerReason = MO_TxEventTriggerReason_UNDEFINED;

        // tx should be started?
        if (txService.isTxStartPoint(TxStartStopPoint::PowerPathClosed) &&
                    (!connectorPluggedInput || connectorPluggedInput(evseId, connectorPluggedInputUserData)) &&
                    transaction && transaction->isAuthorizationActive && transaction->isAuthorized) {
            txStartCondition = true;
            if (transaction->remoteStartId >= 0) {
                triggerReason = MO_TxEventTriggerReason_RemoteStart;
            } else {
                triggerReason = MO_TxEventTriggerReason_CablePluggedIn;
            }
        } else if (txService.isTxStartPoint(TxStartStopPoint::Authorized) &&
                    transaction && transaction->isAuthorizationActive && transaction->isAuthorized) {
            txStartCondition = true;
            if (transaction->remoteStartId >= 0) {
                triggerReason = MO_TxEventTriggerReason_RemoteStart;
            } else {
                triggerReason = MO_TxEventTriggerReason_Authorized;
            }
        } else if (txService.isTxStartPoint(TxStartStopPoint::EVConnected) &&
                    connectorPluggedInput && connectorPluggedInput(evseId, connectorPluggedInputUserData)) {
            txStartCondition = true;
            triggerReason = MO_TxEventTriggerReason_CablePluggedIn;
        } else if (txService.isTxStartPoint(TxStartStopPoint::EnergyTransfer) &&
                    (evReadyInput || evseReadyInput) && // at least one of the two defined
                    (!evReadyInput || evReadyInput(evseId, evReadyInputUserData)) &&
                    (!evseReadyInput || evseReadyInput(evseId, evseReadyInputUserData))) {
            txStartCondition = true;
            triggerReason = MO_TxEventTriggerReason_ChargingStateChanged;
        }

        if (txStartCondition &&
                (!transaction || (transaction->active && !transaction->started)) &&
                (!startTxReadyInput || startTxReadyInput(evseId, startTxReadyInputUserData))) {
            // start tx

            if (!transaction) {
                beginTransaction();
                if (!transaction) {
                    // OOM
                    return;
                }
                if (evseId > 0) {
                    transaction->notifyEvseId = true;
                }
            }

            txEvent = txStore.createTransactionEvent(*transaction);
            if (!txEvent) {
                // OOM
                return;
            }

            txEvent->eventType = TransactionEventData::Type::Started;
            txEvent->triggerReason = triggerReason;
        }
    }

    //General Metering behavior. There is another section for TxStarted, Updated and TxEnded MeterValues
    std::unique_ptr<MicroOcpp::MeterValue> mvTxUpdated;

    if (transaction) {

        int32_t dtLastSampleTimeTxUpdated;
        if (!transaction->lastSampleTimeTxUpdated.isDefined() || !clock.delta(clock.getUptime(), transaction->lastSampleTimeTxUpdated, dtLastSampleTimeTxUpdated)) {
            dtLastSampleTimeTxUpdated = 0;
            transaction->lastSampleTimeTxUpdated = clock.getUptime();
        }

        if (txService.sampledDataTxUpdatedInterval->getInt() > 0 && dtLastSampleTimeTxUpdated >= txService.sampledDataTxUpdatedInterval->getInt()) {
            transaction->lastSampleTimeTxUpdated = clock.getUptime();
            mvTxUpdated = meteringEvse->takeTxUpdatedMeterValue();
        }

        int32_t dtLastSampleTimeTxEnded;
        if (!transaction->lastSampleTimeTxEnded.isDefined() || !clock.delta(clock.getUptime(), transaction->lastSampleTimeTxEnded, dtLastSampleTimeTxEnded)) {
            dtLastSampleTimeTxEnded = 0;
            transaction->lastSampleTimeTxEnded = clock.getUptime();
        }

        if (transaction->started && !transaction->stopped &&
                txService.sampledDataTxEndedInterval->getInt() > 0 &&
                dtLastSampleTimeTxEnded >= txService.sampledDataTxEndedInterval->getInt()) {
            transaction->lastSampleTimeTxEnded = clock.getUptime();
            auto mvTxEnded = meteringEvse->takeTxEndedMeterValue(MO_ReadingContext_SamplePeriodic);
            if (mvTxEnded) {
                transaction->addSampledDataTxEnded(std::move(mvTxEnded));
            }
        }
    }

    if (transaction) {
        // update tx?

        bool txUpdateCondition = false;

        MO_TxEventTriggerReason triggerReason = MO_TxEventTriggerReason_UNDEFINED;

        auto chargingState = getChargingState();

        if (chargingState != trackChargingState) {
            txUpdateCondition = true;
            triggerReason = MO_TxEventTriggerReason_ChargingStateChanged;
            transaction->notifyChargingState = true;
        }
        trackChargingState = chargingState;

        if ((transaction->isAuthorizationActive && transaction->isAuthorized) && !transaction->trackAuthorized) {
            transaction->trackAuthorized = true;
            txUpdateCondition = true;
            if (transaction->remoteStartId >= 0) {
                triggerReason = MO_TxEventTriggerReason_RemoteStart;
            } else {
                triggerReason = MO_TxEventTriggerReason_Authorized;
            }
        } else if (connectorPluggedInput && connectorPluggedInput(evseId, connectorPluggedInputUserData) && !transaction->trackEvConnected) {
            transaction->trackEvConnected = true;
            txUpdateCondition = true;
            triggerReason = MO_TxEventTriggerReason_CablePluggedIn;
        } else if (connectorPluggedInput && !connectorPluggedInput(evseId, connectorPluggedInputUserData) && transaction->trackEvConnected) {
            transaction->trackEvConnected = false;
            txUpdateCondition = true;
            triggerReason = MO_TxEventTriggerReason_EVCommunicationLost;
        } else if (!(transaction->isAuthorizationActive && transaction->isAuthorized) && transaction->trackAuthorized) {
            transaction->trackAuthorized = false;
            txUpdateCondition = true;
            triggerReason = MO_TxEventTriggerReason_StopAuthorized;
        } else if (mvTxUpdated) {
            txUpdateCondition = true;
            triggerReason = MO_TxEventTriggerReason_MeterValuePeriodic;
        } else if (evReadyInput && evReadyInput(evseId, evReadyInputUserData) && !transaction->trackPowerPathClosed) {
            transaction->trackPowerPathClosed = true;
        } else if (evReadyInput && !evReadyInput(evseId, evReadyInputUserData) && transaction->trackPowerPathClosed) {
            transaction->trackPowerPathClosed = false;
        }

        if (txUpdateCondition && !txEvent && transaction->started && !transaction->stopped) {
            // yes, updated

            txEvent = txStore.createTransactionEvent(*transaction);
            if (!txEvent) {
                // OOM
                return;
            }

            txEvent->eventType = TransactionEventData::Type::Updated;
            txEvent->triggerReason = triggerReason;
        }
    }

    if (txEvent) {
        txEvent->timestamp = clock.now();
        if (transaction->notifyChargingState) {
            txEvent->chargingState = getChargingState();
            transaction->notifyChargingState = false;
        }
        if (transaction->notifyEvseId) {
            txEvent->evse = EvseId(evseId, 1);
            transaction->notifyEvseId = false;
        }
        if (transaction->notifyRemoteStartId) {
            txEvent->remoteStartId = transaction->remoteStartId;
            transaction->notifyRemoteStartId = false;
        }
        if (txEvent->eventType == TransactionEventData::Type::Started) {
            auto mvTxStarted = meteringEvse->takeTxStartedMeterValue();
            if (mvTxStarted) {
                txEvent->meterValue.push_back(std::move(mvTxStarted));
            }
            auto mvTxEnded = meteringEvse->takeTxEndedMeterValue(MO_ReadingContext_TransactionBegin);
            if (mvTxEnded) {
                transaction->addSampledDataTxEnded(std::move(mvTxEnded));
            }
            transaction->lastSampleTimeTxEnded = clock.getUptime();
            transaction->lastSampleTimeTxUpdated = clock.getUptime();
        } else if (txEvent->eventType == TransactionEventData::Type::Ended) {
            auto mvTxEnded = meteringEvse->takeTxEndedMeterValue(MO_ReadingContext_TransactionEnd);
            if (mvTxEnded) {
                transaction->addSampledDataTxEnded(std::move(mvTxEnded));
            }
            transaction->lastSampleTimeTxEnded = clock.getUptime();
        }
        if (mvTxUpdated) {
            txEvent->meterValue.push_back(std::move(mvTxUpdated));
        }

        if (transaction->notifyStopIdToken && transaction->stopIdToken) {
            txEvent->idToken = std::unique_ptr<IdToken>(new IdToken(*transaction->stopIdToken.get(), getMemoryTag()));
            transaction->notifyStopIdToken = false;
        }  else if (transaction->notifyIdToken) {
            txEvent->idToken = std::unique_ptr<IdToken>(new IdToken(transaction->idToken, getMemoryTag()));
            transaction->notifyIdToken = false;
        }
    }

    if (txEvent) {
        if (txEvent->eventType == TransactionEventData::Type::Started) {
            transaction->started = true;
            transaction->startTimestamp = txEvent->timestamp;
        } else if (txEvent->eventType == TransactionEventData::Type::Ended) {
            transaction->stopped = true;
        }
    }

    if (txEvent) {
        txEvent->opNr = context.getMessageService().getNextOpNr();
        MO_DBG_DEBUG("enqueueing new txEvent at opNr %u", txEvent->opNr);
    }

    if (txEvent) {
        txStore.commit(txEvent.get());
    }

    if (txEvent) {
        if (txEvent->eventType == TransactionEventData::Type::Started) {
            updateTxNotification(MO_TxNotification_StartTx);
        } else if (txEvent->eventType == TransactionEventData::Type::Ended) {
            updateTxNotification(MO_TxNotification_StartTx);
        }
    }

    //try to pass ownership to front txEvent immediatley
    if (txEvent && !txEventFront &&
            transaction->txNr == txNrFront &&
            !transaction->seqNos.empty() && transaction->seqNos.front() == txEvent->seqNo) {

        //txFront set up?
        if (!txFront) {
            txFront = transaction.get();
        }

        //keep txEvent loaded (otherwise ReqEmitter would load it again from flash)
        MO_DBG_DEBUG("new txEvent is front element");
        txEventFront = std::move(txEvent);
    }
}

void TransactionServiceEvse::setConnectorPluggedInput(bool (*connectorPlugged)(unsigned int, void*), void *userData) {
    this->connectorPluggedInput = connectorPlugged;
    this->connectorPluggedInputUserData = userData;
}

void TransactionServiceEvse::setEvReadyInput(bool (*evReady)(unsigned int, void*), void *userData) {
    this->evReadyInput = evReady;
    this->evReadyInputUserData = userData;
}

void TransactionServiceEvse::setEvseReadyInput(bool (*evseReady)(unsigned int, void*), void *userData) {
    this->evseReadyInput = evseReady;
    this->evseReadyInputUserData = userData;
}

void TransactionServiceEvse::setStartTxReadyInput(bool (*startTxReady)(unsigned int, void*), void *userData) {
    this->startTxReadyInput = startTxReady;
    this->startTxReadyInputUserData = userData;
}

void TransactionServiceEvse::setStopTxReadyInput(bool (*stopTxReady)(unsigned int, void*), void *userData) {
    this->stopTxReadyInput = stopTxReady;
    this->stopTxReadyInputUserData = userData;
}

void TransactionServiceEvse::setTxNotificationOutput(void (*txNotificationOutput)(MO_TxNotification, unsigned int, void*), void *userData) {
    this->txNotificationOutput = txNotificationOutput;
    this->txNotificationOutputUserData = userData;
}

void TransactionServiceEvse::updateTxNotification(MO_TxNotification event) {
    if (txNotificationOutput) {
        txNotificationOutput(event, evseId, txNotificationOutputUserData);
    }
}

bool TransactionServiceEvse::beginAuthorization(IdToken idToken, bool validateIdToken, IdToken groupIdToken) {
    MO_DBG_DEBUG("begin auth: %s", idToken.get());

    if (transaction && transaction->isAuthorizationActive) {
        MO_DBG_WARN("tx process still running. Please call endTransaction(...) before");
        return false;
    }

    if (*groupIdToken.get()) {
        MO_DBG_WARN("groupIdToken not supported");
    }

    if (!transaction) {
        beginTransaction();
        if (!transaction) {
            MO_DBG_ERR("could not allocate Tx");
            return false;
        }
        if (evseId > 0) {
            transaction->notifyEvseId = true;
        }
    }

    transaction->isAuthorizationActive = true;
    transaction->idToken = idToken;
    transaction->beginTimestamp = clock.now();

    if (validateIdToken) {
        auto authorize = makeRequest(context, new Authorize(context.getModel201(), idToken));
        if (!authorize) {
            // OOM
            abortTransaction();
            return false;
        }

        char txId [sizeof(transaction->transactionId)]; //capture txId to check if transaction reference is still the same
        snprintf(txId, sizeof(txId), "%s", transaction->transactionId);

        authorize->setOnReceiveConf([this, txId] (JsonObject response) {
            auto tx = getTransaction();
            if (!tx || strcmp(tx->transactionId, txId)) {
                MO_DBG_INFO("dangling Authorize -- discard");
                return;
            }

            if (strcmp(response["idTokenInfo"]["status"] | "_Undefined", "Accepted")) {
                MO_DBG_DEBUG("Authorize rejected (%s), abort tx process", tx->idToken.get());
                tx->isDeauthorized = true;
                txStore.commit(tx);

                updateTxNotification(MO_TxNotification_AuthorizationRejected);
                return;
            }

            MO_DBG_DEBUG("Authorized tx with validation (%s)", tx->idToken.get());
            tx->isAuthorized = true;
            tx->notifyIdToken = true;
            txStore.commit(tx);

            updateTxNotification(MO_TxNotification_Authorized);
        });
        authorize->setOnAbort([this, txId] () {
            auto tx = getTransaction();
            if (!tx || strcmp(tx->transactionId, txId)) {
                MO_DBG_INFO("dangling Authorize -- discard");
                return;
            }

            MO_DBG_DEBUG("Authorize timeout (%s)", tx->idToken.get());
            tx->isDeauthorized = true;
            txStore.commit(tx);

            updateTxNotification(MO_TxNotification_AuthorizationTimeout);
        });
        authorize->setTimeout(20);
        context.getMessageService().sendRequest(std::move(authorize));
    } else {
        MO_DBG_DEBUG("Authorized tx directly (%s)", transaction->idToken.get());
        transaction->isAuthorized = true;
        transaction->notifyIdToken = true;
        txStore.commit(transaction.get());

        updateTxNotification(MO_TxNotification_Authorized);
    }

    return true;
}
bool TransactionServiceEvse::endAuthorization(IdToken idToken, bool validateIdToken, IdToken groupIdToken) {

    if (!transaction || !transaction->isAuthorizationActive) {
        //transaction already ended / not active anymore
        return false;
    }

    if (*groupIdToken.get()) {
        MO_DBG_WARN("groupIdToken not supported");
    }

    MO_DBG_DEBUG("End session started by idTag %s",
                            transaction->idToken.get());
    
    if (transaction->idToken.equals(idToken)) {
        // use same idToken like tx start
        transaction->isAuthorizationActive = false;
        transaction->notifyIdToken = true;
        txStore.commit(transaction.get());

        updateTxNotification(MO_TxNotification_Authorized);
    } else if (!validateIdToken) {
        transaction->stopIdToken = std::unique_ptr<IdToken>(new IdToken(idToken, getMemoryTag()));
        transaction->isAuthorizationActive = false;
        transaction->notifyStopIdToken = true;
        txStore.commit(transaction.get());

        updateTxNotification(MO_TxNotification_Authorized);
    } else {
        // use a different idToken for stopping the tx

        auto authorize = makeRequest(context, new Authorize(context.getModel201(), idToken));
        if (!authorize) {
            // OOM
            abortTransaction();
            return false;
        }

        char txId [sizeof(transaction->transactionId)]; //capture txId to check if transaction reference is still the same
        snprintf(txId, sizeof(txId), "%s", transaction->transactionId);

        authorize->setOnReceiveConf([this, txId, idToken] (JsonObject response) {
            auto tx = getTransaction();
            if (!tx || strcmp(tx->transactionId, txId)) {
                MO_DBG_INFO("dangling Authorize -- discard");
                return;
            }

            if (strcmp(response["idTokenInfo"]["status"] | "_Undefined", "Accepted")) {
                MO_DBG_DEBUG("Authorize rejected (%s), don't stop tx", idToken.get());

                updateTxNotification(MO_TxNotification_AuthorizationRejected);
                return;
            }

            MO_DBG_DEBUG("Authorized transaction stop (%s)", idToken.get());

            tx->stopIdToken = std::unique_ptr<IdToken>(new IdToken(idToken, getMemoryTag()));
            if (!tx->stopIdToken) {
                // OOM
                if (tx->active) {
                    abortTransaction();
                }
                return;
            }

            tx->isAuthorizationActive = false;
            tx->notifyStopIdToken = true;
            txStore.commit(tx);

            updateTxNotification(MO_TxNotification_Authorized);
        });
        authorize->setOnTimeout([this, txId] () {
            auto tx = getTransaction();
            if (!tx || strcmp(tx->transactionId, txId)) {
                MO_DBG_INFO("dangling Authorize -- discard");
                return;
            }

            updateTxNotification(MO_TxNotification_AuthorizationTimeout);
        });
        authorize->setTimeout(20);
        context.getMessageService().sendRequest(std::move(authorize));
    }

    return true;
}

bool TransactionServiceEvse::abortTransaction(MO_TxStoppedReason stoppedReason, MO_TxEventTriggerReason stopTrigger) {
    return endTransaction(stoppedReason, stopTrigger);
}

Transaction *TransactionServiceEvse::getTransaction() {
    return transaction.get();
}

bool TransactionServiceEvse::ocppPermitsCharge() {
    return transaction &&
           transaction->active &&
           transaction->isAuthorizationActive &&
           transaction->isAuthorized &&
           !transaction->isDeauthorized;
}

TriggerMessageStatus TransactionServiceEvse::triggerTransactionEvent() {
    //F06.FR.07: The Charging Station SHALL send a TransactionEventRequest to the CSMS with
    //triggerReason = Trigger, transactionInfo with at least the chargingState, and meterValue
    //with the most recent measurements for all measurands configured in Configuration Variable:
    //SampledDataTxUpdatedMeasurands.

    if (!transaction) {
        return TriggerMessageStatus::Rejected;
    }

    auto txEvent = txStore.createTransactionEvent(*transaction);
    if (!txEvent) {
        MO_DBG_ERR("OOM");
        return TriggerMessageStatus::ERR_INTERNAL;
    }

    txEvent->eventType = TransactionEventData::Type::Updated;
    txEvent->timestamp = clock.now();
    txEvent->triggerReason = MO_TxEventTriggerReason_Trigger;
    txEvent->chargingState = getChargingState();
    txEvent->evse = evseId;

    auto mvTriggerMessage = meteringEvse->takeTriggeredMeterValues();
    if (mvTriggerMessage) {
        txEvent->meterValue.push_back(std::move(mvTriggerMessage));
    }

    txEvent->opNr = context.getMessageService().getNextOpNr();
    MO_DBG_DEBUG("enqueueing triggered txEvent at opNr %u", txEvent->opNr);

    if (!txStore.commit(txEvent.get())) {
        MO_DBG_ERR("could not commit triggered txEvent");
        return TriggerMessageStatus::Rejected;
    }

    //try to pass ownership to front txEvent immediatley
    if (!txEventFront &&
            transaction->txNr == txNrFront &&
            !transaction->seqNos.empty() && transaction->seqNos.front() == txEvent->seqNo) {

        //txFront set up?
        if (!txFront) {
            txFront = transaction.get();
        }

        //keep txEvent loaded (otherwise ReqEmitter would load it again from flash)
        MO_DBG_DEBUG("new txEvent is front element");
        txEventFront = std::move(txEvent);
    }

    return TriggerMessageStatus::Accepted;
}

unsigned int TransactionServiceEvse::getFrontRequestOpNr() {

    if (txEventFront) {
        return txEventFront->opNr;
    }

    /*
     * Advance front transaction?
     */

    unsigned int txSize = (txNrEnd + MO_TXNR_MAX - txNrFront) % MO_TXNR_MAX;

    if (txFront && txSize == 0) {
        //catch edge case where txBack has been rolled back and txFront was equal to txBack
        MO_DBG_DEBUG("collect front transaction %u-%u after tx rollback", evseId, txFront->txNr);
        MO_DBG_VERBOSE("txNrBegin=%u, txNrFront=%u, txNrEnd=%u", txNrBegin, txNrFront, txNrEnd);
        txEventFront = nullptr;
        txFrontCache = nullptr;
        txFront = nullptr;
    }

    for (unsigned int i = 0; i < txSize; i++) {

        if (!txFront) {
            if (transaction && transaction->txNr == txNrFront) {
                txFront = transaction.get();
            } else {
                txFrontCache = txStore.loadTransaction(txNrFront);
                txFront = txFrontCache.get();
            }

            if (txFront) {
                MO_DBG_DEBUG("load front transaction %u-%u", evseId, txFront->txNr);
                (void)0;
            }
        }

        if (!txFront || (txFront && ((!txFront->active && !txFront->started) || (txFront->stopped && txFront->seqNos.empty()) || txFront->silent))) {
            //advance front
            MO_DBG_DEBUG("collect front transaction %u-%u", evseId, txNrFront);
            txEventFront = nullptr;
            txFrontCache = nullptr;
            txFront = nullptr;
            txNrFront = (txNrFront + 1) % MO_TXNR_MAX;
            MO_DBG_VERBOSE("txNrBegin=%u, txNrFront=%u, txNrEnd=%u", txNrBegin, txNrFront, txNrEnd);
        } else {
            //front is accurate. Done here
            break;
        }
    }

    if (txFront && !txFront->seqNos.empty()) {
        MO_DBG_DEBUG("load front txEvent %u-%u-%u from flash", evseId, txFront->txNr, txFront->seqNos.front());
        txEventFront = txStore.loadTransactionEvent(*txFront, txFront->seqNos.front());
    }

    if (txEventFront) {
        return txEventFront->opNr;
    }

    return NoOperation;
}

std::unique_ptr<Request> TransactionServiceEvse::fetchFrontRequest() {

    if (!txEventFront) {
        return nullptr;
    }

    if (txFront && txFront->silent) {
        return nullptr;
    }

    char dummyTimestamp [MO_JSONDATE_SIZE];
    if (txEventFront->seqNo == 0 &&
            !clock.toJsonString(txEventFront->timestamp, dummyTimestamp, sizeof(dummyTimestamp))) {

        //time not set, cannot be restored anymore -> invalid tx
        MO_DBG_ERR("cannot recover tx from previous power cycle");

        txFront->silent = true;
        txFront->active = false;
        txStore.commit(txFront);

        //clean txEvents early
        auto seqNos = txFront->seqNos;
        for (size_t i = 0; i < seqNos.size(); i++) {
            txStore.remove(*txFront, seqNos[i]);
        }
        //last remove should keep tx201 file with only tx record and without txEvent

        //next getFrontRequestOpNr() call will collect txFront
        return nullptr;
    }

    if ((int)txEventFront->attemptNr >= txService.messageAttemptsTransactionEventInt->getInt()) {
        MO_DBG_WARN("exceeded TransactionMessageAttempts. Discard txEvent");

        txStore.remove(*txFront, txEventFront->seqNo);
        txEventFront = nullptr;
        return nullptr;
    }

    int32_t dtLastAttempt;
    if (!clock.delta(clock.now(), txEventFront->attemptTime, dtLastAttempt)) {
        MO_DBG_ERR("internal error");
        dtLastAttempt = MO_MAX_TIME;
    }

    if (dtLastAttempt < (int)txEventFront->attemptNr * std::max(0, txService.messageAttemptIntervalTransactionEventInt->getInt())) {
        return nullptr;
    }

    if (txEventFrontIsRequested) {
        //ensure that only one TransactionEvent request is being executed at the same time
        return nullptr;
    }

    txEventFront->attemptNr++;
    txEventFront->attemptTime = clock.now();
    txStore.commit(txEventFront.get());

    auto txEventRequest = makeRequest(context, new TransactionEvent(context, txEventFront.get()));
    txEventRequest->setOnReceiveConf([this] (JsonObject) {
        MO_DBG_DEBUG("completed front txEvent");
        txStore.remove(*txFront, txEventFront->seqNo);
        txEventFront = nullptr;
        txEventFrontIsRequested = false;
    });
    txEventRequest->setOnAbort([this] () {
        MO_DBG_DEBUG("unsuccessful front txEvent");
        txEventFrontIsRequested = false;
    });
    txEventRequest->setTimeout(std::min(20, std::max(5, txService.messageAttemptIntervalTransactionEventInt->getInt())));

    txEventFrontIsRequested = true;

    return txEventRequest;
}

bool TransactionService::isTxStartPoint(TxStartStopPoint check) {
    for (auto& v : txStartPointParsed) {
        if (v == check) {
            return true;
        }
    }
    return false;
}
bool TransactionService::isTxStopPoint(TxStartStopPoint check) {
    for (auto& v : txStopPointParsed) {
        if (v == check) {
            return true;
        }
    }
    return false;
}

bool TransactionService::parseTxStartStopPoint(const char *csl, Vector<TxStartStopPoint>& dst) {
    dst.clear();

    while (*csl == ',') {
        csl++;
    }

    while (*csl) {
        if (!strncmp(csl, "ParkingBayOccupancy", sizeof("ParkingBayOccupancy") - 1)
                && (csl[sizeof("ParkingBayOccupancy") - 1] == '\0' || csl[sizeof("ParkingBayOccupancy") - 1] == ',')) {
            dst.push_back(TxStartStopPoint::ParkingBayOccupancy);
            csl += sizeof("ParkingBayOccupancy") - 1;
        } else if (!strncmp(csl, "EVConnected", sizeof("EVConnected") - 1)
                && (csl[sizeof("EVConnected") - 1] == '\0' || csl[sizeof("EVConnected") - 1] == ',')) {
            dst.push_back(TxStartStopPoint::EVConnected);
            csl += sizeof("EVConnected") - 1;
        } else if (!strncmp(csl, "Authorized", sizeof("Authorized") - 1)
                && (csl[sizeof("Authorized") - 1] == '\0' || csl[sizeof("Authorized") - 1] == ',')) {
            dst.push_back(TxStartStopPoint::Authorized);
            csl += sizeof("Authorized") - 1;
        } else if (!strncmp(csl, "DataSigned", sizeof("DataSigned") - 1)
                && (csl[sizeof("DataSigned") - 1] == '\0' || csl[sizeof("DataSigned") - 1] == ',')) {
            dst.push_back(TxStartStopPoint::DataSigned);
            csl += sizeof("DataSigned") - 1;
        } else if (!strncmp(csl, "PowerPathClosed", sizeof("PowerPathClosed") - 1)
                && (csl[sizeof("PowerPathClosed") - 1] == '\0' || csl[sizeof("PowerPathClosed") - 1] == ',')) {
            dst.push_back(TxStartStopPoint::PowerPathClosed);
            csl += sizeof("PowerPathClosed") - 1;
        } else if (!strncmp(csl, "EnergyTransfer", sizeof("EnergyTransfer") - 1)
                && (csl[sizeof("EnergyTransfer") - 1] == '\0' || csl[sizeof("EnergyTransfer") - 1] == ',')) {
            dst.push_back(TxStartStopPoint::EnergyTransfer);
            csl += sizeof("EnergyTransfer") - 1;
        } else {
            MO_DBG_ERR("unkown TxStartStopPoint");
            dst.clear();
            return false;
        }

        while (*csl == ',') {
            csl++;
        }
    }

    return true;
}

namespace MicroOcpp {

bool validateTxStartStopPoint(const char *value, void *userPtr) {
    auto txService = static_cast<TransactionService*>(userPtr);

    auto validated = makeVector<TxStartStopPoint>("v201.Transactions.TransactionService");
    return txService->parseTxStartStopPoint(value, validated);
}

bool validateUnsignedInt(int val, void*) {
    return val >= 0;
}

bool disabledInput(unsigned int, void*) {
    return false;
}

} //namespace MicroOcpp

TransactionService::TransactionService(Context& context) :
        MemoryManaged("v201.Transactions.TransactionService"),
        context(context),
        txStore(context),
        txStartPointParsed(makeVector<TxStartStopPoint>(getMemoryTag())),
        txStopPointParsed(makeVector<TxStartStopPoint>(getMemoryTag())) {

}

TransactionService::~TransactionService() {
    for (unsigned int i = 0; i < MO_NUM_EVSEID; i++) {
        delete evses[i];
        evses[i] = nullptr;
    }
}

bool TransactionService::setup() {

    filesystem = context.getFilesystem();
    if (!filesystem) {
        MO_DBG_DEBUG("volatile mode");
    }

    rngCb = context.getRngCb();
    if (!rngCb) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    if (!txStore.setup()) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    auto varService = context.getModel201().getVariableService();
    if (!varService) {
        return false;
    }

    txStartPointString = varService->declareVariable<const char*>("TxCtrlr", "TxStartPoint", "PowerPathClosed");
    txStopPointString  = varService->declareVariable<const char*>("TxCtrlr", "TxStopPoint",  "PowerPathClosed");
    stopTxOnInvalidIdBool = varService->declareVariable<bool>("TxCtrlr", "StopTxOnInvalidId", true);
    stopTxOnEVSideDisconnectBool = varService->declareVariable<bool>("TxCtrlr", "StopTxOnEVSideDisconnect", true);
    evConnectionTimeOutInt = varService->declareVariable<int>("TxCtrlr", "EVConnectionTimeOut", 30);
    sampledDataTxUpdatedInterval = varService->declareVariable<int>("SampledDataCtrlr", "TxUpdatedInterval", 0);
    sampledDataTxEndedInterval = varService->declareVariable<int>("SampledDataCtrlr", "TxEndedInterval", 0);
    messageAttemptsTransactionEventInt = varService->declareVariable<int>("OCPPCommCtrlr", "MessageAttempts", 3);
    messageAttemptIntervalTransactionEventInt = varService->declareVariable<int>("OCPPCommCtrlr", "MessageAttemptInterval", 60);
    silentOfflineTransactionsBool = varService->declareVariable<bool>("CustomizationCtrlr", "SilentOfflineTransactions", false);

    if (!txStartPointString ||
            !txStopPointString ||
            !stopTxOnInvalidIdBool ||
            !stopTxOnEVSideDisconnectBool ||
            !evConnectionTimeOutInt ||
            !sampledDataTxUpdatedInterval ||
            !sampledDataTxEndedInterval ||
            !messageAttemptsTransactionEventInt ||
            !messageAttemptIntervalTransactionEventInt ||
            !silentOfflineTransactionsBool) {
        MO_DBG_ERR("failure to declare variables");
        return false;
    }

    varService->declareVariable<bool>("AuthCtrlr", "AuthorizeRemoteStart", false, Mutability::ReadOnly, false);
    varService->declareVariable<bool>("AuthCtrlr", "LocalAuthorizeOffline", false, Mutability::ReadOnly, false);

    varService->registerValidator<const char*>("TxCtrlr", "TxStartPoint", validateTxStartStopPoint, this);
    varService->registerValidator<const char*>("TxCtrlr", "TxStopPoint", validateTxStartStopPoint, this);
    varService->registerValidator<int>("SampledDataCtrlr", "TxUpdatedInterval", validateUnsignedInt);
    varService->registerValidator<int>("SampledDataCtrlr", "TxEndedInterval", validateUnsignedInt);

    #if MO_ENABLE_MOCK_SERVER
    context.getMessageService().registerOperation("Authorize", nullptr, Authorize::writeMockConf);
    context.getMessageService().registerOperation("TransactionEvent", TransactionEvent::onRequestMock, TransactionEvent::writeMockConf);
    #endif //MO_ENABLE_MOCK_SERVER

    auto rcService = context.getModel201().getRemoteControlService();
    if (!rcService) {
        MO_DBG_ERR("initialization error");
        return false;
    }

    rcService->addTriggerMessageHandler("TransactionEvent", [] (Context& context, int evseId) -> TriggerMessageStatus {
        TriggerMessageStatus status = TriggerMessageStatus::Rejected;
        auto txSvc = context.getModel201().getTransactionService();

        unsigned int evseIdRangeBegin = 1;
        unsigned int evseIdRangeEnd = context.getModel201().getNumEvseId();

        if (evseId > 0) {
            //send for one evseId only
            evseIdRangeBegin = (unsigned int)evseId;
            evseIdRangeEnd = evseIdRangeBegin + 1;
        }

        for (unsigned i = evseIdRangeBegin; i < evseIdRangeEnd; i++) {
            auto txSvcEvse = txSvc ? txSvc->getEvse(i) : nullptr;
            if (!txSvcEvse) {
                return TriggerMessageStatus::ERR_INTERNAL;
            }
            
            auto ret = txSvcEvse->triggerTransactionEvent();

            bool abortLoop = false;

            switch (ret) {
                case TriggerMessageStatus::Accepted:
                    //Accepted takes precedence over Rejected
                    status = TriggerMessageStatus::Accepted;
                    break;
                case TriggerMessageStatus::Rejected:
                    //if one EVSE rejects, but another accepts, respond with Accepted
                    break;
                case TriggerMessageStatus::ERR_INTERNAL:
                case TriggerMessageStatus::NotImplemented:
                    status = TriggerMessageStatus::ERR_INTERNAL;
                    abortLoop = true;
                    break;
            }

            if (abortLoop) {
                break;
            }
        }

        return status;
    });

    numEvseId = context.getModel201().getNumEvseId();
    for (unsigned int i = 0; i < numEvseId; i++) {
        if (!getEvse(i) || !getEvse(i)->setup()) {
            MO_DBG_ERR("initialization error");
            return false;
        }
    }

    //make sure EVSE 0 will only trigger transactions if TxStartPoint is Authorized
    if (evses[0]) {
        evses[0]->connectorPluggedInput = disabledInput;
        evses[0]->evReadyInput = disabledInput;
        evses[0]->evseReadyInput = disabledInput;
    }
    
    return true;
}

void TransactionService::loop() {
    for (unsigned int evseId = 0; evseId < MO_NUM_EVSEID && evses[evseId]; evseId++) {
        evses[evseId]->loop();
    }

    if (txStartPointString->getWriteCount() != trackTxStartPoint) {
        parseTxStartStopPoint(txStartPointString->getString(), txStartPointParsed);
    }

    if (txStopPointString->getWriteCount() != trackTxStopPoint) {
        parseTxStartStopPoint(txStopPointString->getString(), txStopPointParsed);
    }

    // assign tx on evseId 0 to an EVSE
    if (evses[0]->transaction) {
        //pending tx on evseId 0
        if (evses[0]->transaction->active) {
            for (unsigned int evseId = 1; evseId < MO_NUM_EVSEID && evses[evseId]; evseId++) {
                if (!evses[evseId]->getTransaction() && 
                        (!evses[evseId]->connectorPluggedInput || evses[evseId]->connectorPluggedInput(evseId, evses[evseId]->connectorPluggedInputUserData))) {
                    MO_DBG_INFO("assign tx to evse %u", evseId);
                    evses[0]->transaction->notifyEvseId = true;
                    evses[0]->transaction->evseId = evseId;
                    evses[evseId]->transaction = std::move(evses[0]->transaction);
                }
            }
        }
    }
}

TransactionServiceEvse *TransactionService::getEvse(unsigned int evseId) {
    if (evseId >= numEvseId) {
        MO_DBG_ERR("evseId out of bound");
        return nullptr;
    }

    auto txStoreEvse = txStore.getEvse(evseId);
    if (!txStoreEvse) {
        MO_DBG_ERR("OOM");
        return nullptr;
    }

    if (!evses[evseId]) {
        evses[evseId] = new TransactionServiceEvse(context, *this, *txStoreEvse, evseId);
        if (!evses[evseId]) {
            MO_DBG_ERR("OOM");
            return nullptr;
        }
    }

    return evses[evseId];
}

#endif //MO_ENABLE_V201
