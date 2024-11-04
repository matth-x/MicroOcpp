// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Model/Transactions/TransactionService.h>

#include <string.h>

#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Operations/Authorize.h>
#include <MicroOcpp/Operations/TransactionEvent.h>
#include <MicroOcpp/Operations/RequestStartTransaction.h>
#include <MicroOcpp/Operations/RequestStopTransaction.h>
#include <MicroOcpp/Debug.h>

#ifndef MO_TX_CLEAN_ABORTED
#define MO_TX_CLEAN_ABORTED 1
#endif

using namespace MicroOcpp;
using namespace MicroOcpp::Ocpp201;

TransactionService::Evse::Evse(Context& context, TransactionService& txService, Ocpp201::TransactionStoreEvse& txStore, unsigned int evseId) :
        MemoryManaged("v201.Transactions.TransactionServiceEvse"),
        context(context),
        txService(txService),
        txStore(txStore),
        evseId(evseId) {

    context.getRequestQueue().addSendQueue(this); //register at RequestQueue as Request emitter

    txStore.discoverStoredTx(txNrBegin, txNrEnd); //initializes txNrBegin and txNrEnd
    txNrFront = txNrBegin;
    MO_DBG_DEBUG("found %u transactions for evseId %u. Internal range from %u to %u (exclusive)", (txNrEnd + MAX_TX_CNT - txNrBegin) % MAX_TX_CNT, evseId, txNrBegin, txNrEnd);

    unsigned int txNrLatest = (txNrEnd + MAX_TX_CNT - 1) % MAX_TX_CNT; //txNr of the most recent tx on flash
    transaction = txStore.loadTransaction(txNrLatest); //returns nullptr if txNrLatest does not exist on flash
}

TransactionService::Evse::~Evse() {
    
}

bool TransactionService::Evse::beginTransaction() {

    if (transaction) {
        MO_DBG_ERR("transaction still running");
        return false;
    }

    std::unique_ptr<Ocpp201::Transaction> tx;

    char txId [sizeof(Ocpp201::Transaction::transactionId)];

    //simple clock-based hash
    int v = context.getModel().getClock().now() - Timestamp(2020,0,0,0,0,0);
    unsigned int h = v;
    h += mocpp_tick_ms();
    h *= 749572633U;
    h %= 24593209U;
    for (size_t i = 0; i < sizeof(tx->transactionId) - 3; i += 2) {
        snprintf(txId + i, 3, "%02X", (uint8_t)h);
        h *= 749572633U;
        h %= 24593209U;
    }

    //clean possible aborted tx
    unsigned int txr = txNrEnd;
    unsigned int txSize = (txNrEnd + MAX_TX_CNT - txNrBegin) % MAX_TX_CNT;
    for (unsigned int i = 0; i < txSize; i++) {
        txr = (txr + MAX_TX_CNT - 1) % MAX_TX_CNT; //decrement by 1

        std::unique_ptr<Ocpp201::Transaction> intermediateTx;

        Ocpp201::Transaction *txhist = nullptr;
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

    txSize = (txNrEnd + MAX_TX_CNT - txNrBegin) % MAX_TX_CNT; //refresh after cleaning txs

    //try to create new transaction
    if (txSize < MO_TXRECORD_SIZE) {
        tx = txStore.createTransaction(txNrEnd, txId);
    }

    if (!tx) {
        //could not create transaction - now, try to replace tx history entry

        unsigned int txl = txNrBegin;
        txSize = (txNrEnd + MAX_TX_CNT - txNrBegin) % MAX_TX_CNT;

        for (unsigned int i = 0; i < txSize; i++) {

            if (tx) {
                //success, finished here
                break;
            }

            //no transaction allocated, delete history entry to make space
            std::unique_ptr<Ocpp201::Transaction> intermediateTx;

            Ocpp201::Transaction *txhist = nullptr;
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
                    txNrBegin = (txl + 1) % MAX_TX_CNT;
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
            txl %= MAX_TX_CNT;
        }
    }

    if (!tx) {
        //couldn't create normal transaction -> check if to start charging without real transaction
        if (txService.silentOfflineTransactionsBool && txService.silentOfflineTransactionsBool->getBool()) {
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

    tx->beginTimestamp = context.getModel().getClock().now();

    if (!txStore.commit(tx.get())) {
        MO_DBG_ERR("fs error");
        return false;
    }

    transaction = std::move(tx);

    txNrEnd = (txNrEnd + 1) % MAX_TX_CNT;
    MO_DBG_DEBUG("advance txNrEnd %u-%u", evseId, txNrEnd);
    MO_DBG_VERBOSE("txNrBegin=%u, txNrFront=%u, txNrEnd=%u", txNrBegin, txNrFront, txNrEnd);

    return true;
}

bool TransactionService::Evse::endTransaction(Ocpp201::Transaction::StoppedReason stoppedReason = Ocpp201::Transaction::StoppedReason::Other, Ocpp201::TransactionEventTriggerReason stopTrigger = Ocpp201::TransactionEventTriggerReason::AbnormalCondition) {

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

void TransactionService::Evse::loop() {

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
            if (connectorPluggedInput()) {
                // if cable has been plugged at least once, EVConnectionTimeout will never get triggered
                transaction->evConnectionTimeoutListen = false;
            }

            if (transaction->active &&
                    transaction->evConnectionTimeoutListen &&
                    transaction->beginTimestamp > MIN_TIME &&
                    txService.evConnectionTimeOutInt && txService.evConnectionTimeOutInt->getInt() > 0 &&
                    !connectorPluggedInput() &&
                    context.getModel().getClock().now() - transaction->beginTimestamp >= txService.evConnectionTimeOutInt->getInt()) {

                MO_DBG_INFO("Session mngt: timeout");
                endTransaction(Ocpp201::Transaction::StoppedReason::Timeout, TransactionEventTriggerReason::EVConnectTimeout);

                updateTxNotification(TxNotification_ConnectionTimeout);
            }

            if (transaction->active &&
                    transaction->isDeauthorized &&
                    !transaction->started &&
                    (txService.isTxStartPoint(TxStartStopPoint::Authorized) || txService.isTxStartPoint(TxStartStopPoint::PowerPathClosed) ||
                     txService.isTxStopPoint(TxStartStopPoint::Authorized)  || txService.isTxStopPoint(TxStartStopPoint::PowerPathClosed))) {
                
                MO_DBG_INFO("Session mngt: Deauthorized before start");
                endTransaction(Ocpp201::Transaction::StoppedReason::DeAuthorized, TransactionEventTriggerReason::Deauthorized);
            }
        }
    }

    std::unique_ptr<TransactionEventData> txEvent;

    bool txStopCondition = false;

    {
        // stop tx?

        TransactionEventTriggerReason triggerReason = TransactionEventTriggerReason::UNDEFINED;
        Ocpp201::Transaction::StoppedReason stoppedReason = Ocpp201::Transaction::StoppedReason::UNDEFINED;

        if (transaction && !transaction->active) {
            // tx ended via endTransaction
            txStopCondition = true;
            triggerReason = transaction->stopTrigger;
            stoppedReason = transaction->stoppedReason;
        } else if ((txService.isTxStopPoint(TxStartStopPoint::EVConnected) ||
                    txService.isTxStopPoint(TxStartStopPoint::PowerPathClosed)) &&
                    connectorPluggedInput && !connectorPluggedInput() &&
                    (txService.stopTxOnEVSideDisconnectBool->getBool() || !transaction || !transaction->started)) {
            txStopCondition = true;
            triggerReason = TransactionEventTriggerReason::EVCommunicationLost;
            stoppedReason = Ocpp201::Transaction::StoppedReason::EVDisconnected;
        } else if ((txService.isTxStopPoint(TxStartStopPoint::Authorized) ||
                    txService.isTxStopPoint(TxStartStopPoint::PowerPathClosed)) &&
                    (!transaction || !transaction->isAuthorizationActive)) {
            // user revoked authorization (or EV or any "local" entity)
            txStopCondition = true;
            triggerReason = TransactionEventTriggerReason::StopAuthorized;
            stoppedReason = Ocpp201::Transaction::StoppedReason::Local;
        } else if (txService.isTxStopPoint(TxStartStopPoint::EnergyTransfer) &&
                    evReadyInput && !evReadyInput()) {
            txStopCondition = true;
            triggerReason = TransactionEventTriggerReason::ChargingStateChanged;
            stoppedReason = Ocpp201::Transaction::StoppedReason::StoppedByEV;
        } else if (txService.isTxStopPoint(TxStartStopPoint::EnergyTransfer) &&
                    (evReadyInput || evseReadyInput) && // at least one of the two defined
                    !(evReadyInput && evReadyInput()) &&
                    !(evseReadyInput && evseReadyInput())) {
            txStopCondition = true;
            triggerReason = TransactionEventTriggerReason::ChargingStateChanged;
            stoppedReason = Ocpp201::Transaction::StoppedReason::Other;
        } else if (txService.isTxStopPoint(TxStartStopPoint::Authorized) &&
                    transaction && transaction->isDeauthorized &&
                    txService.stopTxOnInvalidIdBool->getBool()) {
            // OCPP server rejected authorization
            txStopCondition = true;
            triggerReason = TransactionEventTriggerReason::Deauthorized;
            stoppedReason = Ocpp201::Transaction::StoppedReason::DeAuthorized;
        }

        if (txStopCondition &&
                transaction && transaction->started && transaction->active) {

            MO_DBG_INFO("Session mngt: TxStopPoint reached");
            endTransaction(stoppedReason, triggerReason);
        }

        if (transaction &&
                transaction->started && !transaction->stopped && !transaction->active &&
                (!stopTxReadyInput || stopTxReadyInput())) {
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

        TransactionEventTriggerReason triggerReason = TransactionEventTriggerReason::UNDEFINED;

        // tx should be started?
        if (txService.isTxStartPoint(TxStartStopPoint::PowerPathClosed) &&
                    (!connectorPluggedInput || connectorPluggedInput()) &&
                    transaction && transaction->isAuthorizationActive && transaction->isAuthorized) {
            txStartCondition = true;
            if (transaction->remoteStartId >= 0) {
                triggerReason = TransactionEventTriggerReason::RemoteStart;
            } else {
                triggerReason = TransactionEventTriggerReason::CablePluggedIn;
            }
        } else if (txService.isTxStartPoint(TxStartStopPoint::Authorized) &&
                    transaction && transaction->isAuthorizationActive && transaction->isAuthorized) {
            txStartCondition = true;
            if (transaction->remoteStartId >= 0) {
                triggerReason = TransactionEventTriggerReason::RemoteStart;
            } else {
                triggerReason = TransactionEventTriggerReason::Authorized;
            }
        } else if (txService.isTxStartPoint(TxStartStopPoint::EVConnected) &&
                    connectorPluggedInput && connectorPluggedInput()) {
            txStartCondition = true;
            triggerReason = TransactionEventTriggerReason::CablePluggedIn;
        } else if (txService.isTxStartPoint(TxStartStopPoint::EnergyTransfer) &&
                    (evReadyInput || evseReadyInput) && // at least one of the two defined
                    (!evReadyInput || evReadyInput()) &&
                    (!evseReadyInput || evseReadyInput())) {
            txStartCondition = true;
            triggerReason = TransactionEventTriggerReason::ChargingStateChanged;
        }

        if (txStartCondition &&
                (!transaction || (transaction->active && !transaction->started)) &&
                (!startTxReadyInput || startTxReadyInput())) {
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

    TransactionEventData::ChargingState chargingState = TransactionEventData::ChargingState::Idle;
    if (connectorPluggedInput && !connectorPluggedInput()) {
        chargingState = TransactionEventData::ChargingState::Idle;
    } else if (!transaction || !transaction->isAuthorizationActive || !transaction->isAuthorized) {
        chargingState = TransactionEventData::ChargingState::EVConnected;
    } else if (evseReadyInput && !evseReadyInput()) { 
        chargingState = TransactionEventData::ChargingState::SuspendedEVSE;
    } else if (evReadyInput && !evReadyInput()) {
        chargingState = TransactionEventData::ChargingState::SuspendedEV;
    } else if (ocppPermitsCharge()) {
        chargingState = TransactionEventData::ChargingState::Charging;
    }

    //General Metering behavior. There is another section for TxStarted, Updated and TxEnded MeterValues
    std::unique_ptr<MicroOcpp::Ocpp201::MeterValue> mvTxUpdated;

    if (transaction) {

        if (txService.sampledDataTxUpdatedInterval && txService.sampledDataTxUpdatedInterval->getInt() > 0 && mocpp_tick_ms() - transaction->lastSampleTimeTxUpdated >= (unsigned long)txService.sampledDataTxUpdatedInterval->getInt() * 1000UL) {
            transaction->lastSampleTimeTxUpdated = mocpp_tick_ms();
            auto meteringService = context.getModel().getMeteringServiceV201();
            auto meteringEvse = meteringService ? meteringService->getEvse(evseId) : nullptr;
            mvTxUpdated = meteringEvse ? meteringEvse->takeTxUpdatedMeterValue() : nullptr;
        }

        if (transaction->started && !transaction->stopped &&
                 txService.sampledDataTxEndedInterval && txService.sampledDataTxEndedInterval->getInt() > 0 &&
                 mocpp_tick_ms() - transaction->lastSampleTimeTxEnded >= (unsigned long)txService.sampledDataTxEndedInterval->getInt() * 1000UL) {
            transaction->lastSampleTimeTxEnded = mocpp_tick_ms();
            auto meteringService = context.getModel().getMeteringServiceV201();
            auto meteringEvse = meteringService ? meteringService->getEvse(evseId) : nullptr;
            auto mvTxEnded = meteringEvse ? meteringEvse->takeTxEndedMeterValue(ReadingContext_SamplePeriodic) : nullptr;
            if (mvTxEnded) {
                transaction->addSampledDataTxEnded(std::move(mvTxEnded));
            }
        }
    }

    if (transaction) {
        // update tx?

        bool txUpdateCondition = false;

        TransactionEventTriggerReason triggerReason = TransactionEventTriggerReason::UNDEFINED;

        if (chargingState != trackChargingState) {
            txUpdateCondition = true;
            triggerReason = TransactionEventTriggerReason::ChargingStateChanged;
            transaction->notifyChargingState = true;
        }
        trackChargingState = chargingState;

        if ((transaction->isAuthorizationActive && transaction->isAuthorized) && !transaction->trackAuthorized) {
            transaction->trackAuthorized = true;
            txUpdateCondition = true;
            if (transaction->remoteStartId >= 0) {
                triggerReason = TransactionEventTriggerReason::RemoteStart;
            } else {
                triggerReason = TransactionEventTriggerReason::Authorized;
            }
        } else if (connectorPluggedInput && connectorPluggedInput() && !transaction->trackEvConnected) {
            transaction->trackEvConnected = true;
            txUpdateCondition = true;
            triggerReason = TransactionEventTriggerReason::CablePluggedIn;
        } else if (connectorPluggedInput && !connectorPluggedInput() && transaction->trackEvConnected) {
            transaction->trackEvConnected = false;
            txUpdateCondition = true;
            triggerReason = TransactionEventTriggerReason::EVCommunicationLost;
        } else if (!(transaction->isAuthorizationActive && transaction->isAuthorized) && transaction->trackAuthorized) {
            transaction->trackAuthorized = false;
            txUpdateCondition = true;
            triggerReason = TransactionEventTriggerReason::StopAuthorized;
        } else if (mvTxUpdated) {
            txUpdateCondition = true;
            triggerReason = TransactionEventTriggerReason::MeterValuePeriodic;
        } else if (evReadyInput && evReadyInput() && !transaction->trackPowerPathClosed) {
            transaction->trackPowerPathClosed = true;
        } else if (evReadyInput && !evReadyInput() && transaction->trackPowerPathClosed) {
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
        txEvent->timestamp = context.getModel().getClock().now();
        if (transaction->notifyChargingState) {
            txEvent->chargingState = chargingState;
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
            auto meteringService = context.getModel().getMeteringServiceV201();
            auto meteringEvse = meteringService ? meteringService->getEvse(evseId) : nullptr;
            auto mvTxStarted = meteringEvse ? meteringEvse->takeTxStartedMeterValue() : nullptr;
            if (mvTxStarted) {
                txEvent->meterValue.push_back(std::move(mvTxStarted));
            }
            auto mvTxEnded = meteringEvse ? meteringEvse->takeTxEndedMeterValue(ReadingContext_TransactionBegin) : nullptr;
            if (mvTxEnded) {
                transaction->addSampledDataTxEnded(std::move(mvTxEnded));
            }
            transaction->lastSampleTimeTxEnded = mocpp_tick_ms();
            transaction->lastSampleTimeTxUpdated = mocpp_tick_ms();
        } else if (txEvent->eventType == TransactionEventData::Type::Ended) {
            auto meteringService = context.getModel().getMeteringServiceV201();
            auto meteringEvse = meteringService ? meteringService->getEvse(evseId) : nullptr;
            auto mvTxEnded = meteringEvse ? meteringEvse->takeTxEndedMeterValue(ReadingContext_TransactionEnd) : nullptr;
            if (mvTxEnded) {
                transaction->addSampledDataTxEnded(std::move(mvTxEnded));
            }
            transaction->lastSampleTimeTxEnded = mocpp_tick_ms();
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
        } else if (txEvent->eventType == TransactionEventData::Type::Ended) {
            transaction->stopped = true;
        }
    }

    if (txEvent) {
        txEvent->opNr = context.getRequestQueue().getNextOpNr();
        MO_DBG_DEBUG("enqueueing new txEvent at opNr %u", txEvent->opNr);
    }

    if (txEvent) {
        txStore.commit(txEvent.get());
    }

    if (txEvent) {
        if (txEvent->eventType == TransactionEventData::Type::Started) {
            updateTxNotification(TxNotification_StartTx);
        } else if (txEvent->eventType == TransactionEventData::Type::Ended) {
            updateTxNotification(TxNotification_StartTx);
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

void TransactionService::Evse::setConnectorPluggedInput(std::function<bool()> connectorPlugged) {
    this->connectorPluggedInput = connectorPlugged;
}

void TransactionService::Evse::setEvReadyInput(std::function<bool()> evRequestsEnergy) {
    this->evReadyInput = evRequestsEnergy;
}

void TransactionService::Evse::setEvseReadyInput(std::function<bool()> connectorEnergized) {
    this->evseReadyInput = connectorEnergized;
}

void TransactionService::Evse::setTxNotificationOutput(std::function<void(Ocpp201::Transaction*, TxNotification)> txNotificationOutput) {
    this->txNotificationOutput = txNotificationOutput;
}

void TransactionService::Evse::updateTxNotification(TxNotification event) {
    if (txNotificationOutput) {
        txNotificationOutput(transaction.get(), event);
    }
}

bool TransactionService::Evse::beginAuthorization(IdToken idToken, bool validateIdToken) {
    MO_DBG_DEBUG("begin auth: %s", idToken.get());

    if (transaction && transaction->isAuthorizationActive) {
        MO_DBG_WARN("tx process still running. Please call endTransaction(...) before");
        return false;
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
    transaction->beginTimestamp = context.getModel().getClock().now();

    if (validateIdToken) {
        auto authorize = makeRequest(new Authorize(context.getModel(), idToken));
        if (!authorize) {
            // OOM
            abortTransaction();
            return false;
        }

        char txId [sizeof(transaction->transactionId)]; //capture txId to check if transaction reference is still the same
        snprintf(txId, sizeof(txId), "%s", transaction->transactionId);

        authorize->setOnReceiveConfListener([this, txId] (JsonObject response) {
            auto tx = getTransaction();
            if (!tx || strcmp(tx->transactionId, txId)) {
                MO_DBG_INFO("dangling Authorize -- discard");
                return;
            }

            if (strcmp(response["idTokenInfo"]["status"] | "_Undefined", "Accepted")) {
                MO_DBG_DEBUG("Authorize rejected (%s), abort tx process", tx->idToken.get());
                tx->isDeauthorized = true;
                txStore.commit(tx);

                updateTxNotification(TxNotification_AuthorizationRejected);
                return;
            }

            MO_DBG_DEBUG("Authorized tx with validation (%s)", tx->idToken.get());
            tx->isAuthorized = true;
            tx->notifyIdToken = true;
            txStore.commit(tx);

            updateTxNotification(TxNotification_Authorized);
        });
        authorize->setOnAbortListener([this, txId] () {
            auto tx = getTransaction();
            if (!tx || strcmp(tx->transactionId, txId)) {
                MO_DBG_INFO("dangling Authorize -- discard");
                return;
            }

            MO_DBG_DEBUG("Authorize timeout (%s)", tx->idToken.get());
            tx->isDeauthorized = true;
            txStore.commit(tx);

            updateTxNotification(TxNotification_AuthorizationTimeout);
        });
        authorize->setTimeout(20 * 1000);
        context.initiateRequest(std::move(authorize));
    } else {
        MO_DBG_DEBUG("Authorized tx directly (%s)", transaction->idToken.get());
        transaction->isAuthorized = true;
        transaction->notifyIdToken = true;
        txStore.commit(transaction.get());

        updateTxNotification(TxNotification_Authorized);
    }

    return true;
}
bool TransactionService::Evse::endAuthorization(IdToken idToken, bool validateIdToken) {

    if (!transaction || !transaction->isAuthorizationActive) {
        //transaction already ended / not active anymore
        return false;
    }

    MO_DBG_DEBUG("End session started by idTag %s",
                            transaction->idToken.get());
    
    if (transaction->idToken.equals(idToken)) {
        // use same idToken like tx start
        transaction->isAuthorizationActive = false;
        transaction->notifyIdToken = true;
        txStore.commit(transaction.get());

        updateTxNotification(TxNotification_Authorized);
    } else if (!validateIdToken) {
        transaction->stopIdToken = std::unique_ptr<IdToken>(new IdToken(idToken, getMemoryTag()));
        transaction->isAuthorizationActive = false;
        transaction->notifyStopIdToken = true;
        txStore.commit(transaction.get());

        updateTxNotification(TxNotification_Authorized);
    } else {
        // use a different idToken for stopping the tx

        auto authorize = makeRequest(new Authorize(context.getModel(), idToken));
        if (!authorize) {
            // OOM
            abortTransaction();
            return false;
        }

        char txId [sizeof(transaction->transactionId)]; //capture txId to check if transaction reference is still the same
        snprintf(txId, sizeof(txId), "%s", transaction->transactionId);

        authorize->setOnReceiveConfListener([this, txId, idToken] (JsonObject response) {
            auto tx = getTransaction();
            if (!tx || strcmp(tx->transactionId, txId)) {
                MO_DBG_INFO("dangling Authorize -- discard");
                return;
            }

            if (strcmp(response["idTokenInfo"]["status"] | "_Undefined", "Accepted")) {
                MO_DBG_DEBUG("Authorize rejected (%s), don't stop tx", idToken.get());

                updateTxNotification(TxNotification_AuthorizationRejected);
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

            updateTxNotification(TxNotification_Authorized);
        });
        authorize->setOnTimeoutListener([this, txId] () {
            auto tx = getTransaction();
            if (!tx || strcmp(tx->transactionId, txId)) {
                MO_DBG_INFO("dangling Authorize -- discard");
                return;
            }

            updateTxNotification(TxNotification_AuthorizationTimeout);
        });
        authorize->setTimeout(20 * 1000);
        context.initiateRequest(std::move(authorize));
    }

    return true;
}
bool TransactionService::Evse::abortTransaction(Ocpp201::Transaction::StoppedReason stoppedReason, TransactionEventTriggerReason stopTrigger) {
    return endTransaction(stoppedReason, stopTrigger);
}
MicroOcpp::Ocpp201::Transaction *TransactionService::Evse::getTransaction() {
    return transaction.get();
}

bool TransactionService::Evse::ocppPermitsCharge() {
    return transaction &&
           transaction->active &&
           transaction->isAuthorizationActive &&
           transaction->isAuthorized &&
           !transaction->isDeauthorized;
}

unsigned int TransactionService::Evse::getFrontRequestOpNr() {

    if (txEventFront) {
        return txEventFront->opNr;
    }

    /*
     * Advance front transaction?
     */

    unsigned int txSize = (txNrEnd + MAX_TX_CNT - txNrFront) % MAX_TX_CNT;

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
            txNrFront = (txNrFront + 1) % MAX_TX_CNT;
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

std::unique_ptr<Request> TransactionService::Evse::fetchFrontRequest() {

    if (!txEventFront) {
        return nullptr;
    }

    if (txFront && txFront->silent) {
        return nullptr;
    }

    if (txEventFront->seqNo == 0 &&
            txEventFront->timestamp < MIN_TIME &&
            txEventFront->bootNr != context.getModel().getBootNr()) {
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

    Timestamp nextAttempt = txEventFront->attemptTime +
                            txEventFront->attemptNr * std::max(0, txService.messageAttemptIntervalTransactionEventInt->getInt());

    if (nextAttempt > context.getModel().getClock().now()) {
        return nullptr;
    }

    if (txEventFrontIsRequested) {
        //ensure that only one TransactionEvent request is being executed at the same time
        return nullptr;
    }

    txEventFront->attemptNr++;
    txEventFront->attemptTime = context.getModel().getClock().now();
    txStore.commit(txEventFront.get());

    auto txEventRequest = makeRequest(new TransactionEvent(context.getModel(), txEventFront.get()));
    txEventRequest->setOnReceiveConfListener([this] (JsonObject) {
        MO_DBG_DEBUG("completed front txEvent");
        txStore.remove(*txFront, txEventFront->seqNo);
        txEventFront = nullptr;
        txEventFrontIsRequested = false;
    });
    txEventRequest->setOnAbortListener([this] () {
        MO_DBG_DEBUG("unsuccessful front txEvent");
        txEventFrontIsRequested = false;
    });
    txEventRequest->setTimeout(std::min(20, std::max(5, txService.messageAttemptIntervalTransactionEventInt->getInt())) * 1000);

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

    auto validated = makeVector<TransactionService::TxStartStopPoint>("v201.Transactions.TransactionService");
    return txService->parseTxStartStopPoint(value, validated);
}

bool validateUnsignedInt(int val, void*) {
    return val >= 0;
}

} //namespace MicroOcpp

using namespace MicroOcpp;

TransactionService::TransactionService(Context& context, std::shared_ptr<FilesystemAdapter> filesystem, unsigned int numEvseIds) :
        MemoryManaged("v201.Transactions.TransactionService"),
        context(context),
        txStore(filesystem, numEvseIds),
        txStartPointParsed(makeVector<TxStartStopPoint>(getMemoryTag())),
        txStopPointParsed(makeVector<TxStartStopPoint>(getMemoryTag())) {
    auto varService = context.getModel().getVariableService();

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

    varService->declareVariable<bool>("AuthCtrlr", "AuthorizeRemoteStart", false, Variable::Mutability::ReadOnly, false);

    varService->registerValidator<const char*>("TxCtrlr", "TxStartPoint", validateTxStartStopPoint, this);
    varService->registerValidator<const char*>("TxCtrlr", "TxStopPoint", validateTxStartStopPoint, this);
    varService->registerValidator<int>("SampledDataCtrlr", "TxUpdatedInterval", validateUnsignedInt);
    varService->registerValidator<int>("SampledDataCtrlr", "TxEndedInterval", validateUnsignedInt);

    for (unsigned int evseId = 0; evseId < std::min(numEvseIds, (unsigned int)MO_NUM_EVSEID); evseId++) {
        if (!txStore.getEvse(evseId)) {
            MO_DBG_ERR("initialization error");
            break;
        }
        evses[evseId] = new Evse(context, *this, *txStore.getEvse(evseId), evseId);
    }

    //make sure EVSE 0 will only trigger transactions if TxStartPoint is Authorized
    if (evses[0]) {
        evses[0]->connectorPluggedInput = [] () {return false;};
        evses[0]->evReadyInput = [] () {return false;};
        evses[0]->evseReadyInput = [] () {return false;};
    }
}

TransactionService::~TransactionService() {
    for (unsigned int evseId = 0; evseId < MO_NUM_EVSEID && evses[evseId]; evseId++) {
        delete evses[evseId];
    }
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
                        (!evses[evseId]->connectorPluggedInput || evses[evseId]->connectorPluggedInput())) {
                    MO_DBG_INFO("assign tx to evse %u", evseId);
                    evses[0]->transaction->notifyEvseId = true;
                    evses[0]->transaction->evseId = evseId;
                    evses[evseId]->transaction = std::move(evses[0]->transaction);
                }
            }
        }
    }
}

TransactionService::Evse *TransactionService::getEvse(unsigned int evseId) {
    if (evseId >= MO_NUM_EVSEID) {
        return nullptr;
    }
    return evses[evseId];
}

#endif // MO_ENABLE_V201
