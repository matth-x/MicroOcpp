// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Model/Transactions/TransactionService.h>

#include <string.h>

#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Operations/Authorize.h>
#include <MicroOcpp/Operations/TransactionEvent.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;
using namespace MicroOcpp::Ocpp201;

TransactionService::Evse::Evse(Context& context, TransactionService& txService, unsigned int evseId) :
        context(context), txService(txService), evseId(evseId) {

}

std::unique_ptr<Ocpp201::Transaction> TransactionService::Evse::allocateTransaction() {
    auto tx =  std::unique_ptr<Ocpp201::Transaction>(new Ocpp201::Transaction());
    if (!tx) {
        // OOM
        return nullptr;
    }

    //simple clock-based hash
    int v = context.getModel().getClock().now() - Timestamp(2020,0,0,0,0,0);
    unsigned int h = v;
    h += mocpp_tick_ms();
    h *= 749572633U;
    h %= 24593209U;
    for (size_t i = 0; i < sizeof(tx->transactionId) - 3; i += 2) {
        sprintf(tx->transactionId + i, "%02X", (uint8_t)h);
        h *= 749572633U;
        h %= 24593209U;
    }

    tx->beginTimestamp = context.getModel().getClock().now();

    return tx;
}

void TransactionService::Evse::loop() {

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
                transaction->active = false;
                transaction->stopTrigger = TransactionEventTriggerReason::EVConnectTimeout;
                transaction->stopReason = Ocpp201::Transaction::StopReason::Timeout;
            }
        }
    }

    // TxEvent types
    bool txStarted = false;
    bool txStopped = false;
    bool txUpdated = false;

    TransactionEventTriggerReason triggerReason = TransactionEventTriggerReason::UNDEFINED;
    Ocpp201::Transaction::StopReason stoppedReason = Ocpp201::Transaction::StopReason::UNDEFINED;

    // tx should be updated?
    TransactionEventData::ChargingState chargingState = TransactionEventData::ChargingState::Idle;
    if (transaction) {
        chargingState = TransactionEventData::ChargingState::Charging;

        if (connectorPluggedInput && !connectorPluggedInput()) {
            chargingState = TransactionEventData::ChargingState::Idle;
        } else if (!transaction || !transaction->isAuthorized) {
            chargingState = TransactionEventData::ChargingState::EVConnected;
        } else if (evseReadyInput && !evseReadyInput()) { 
            chargingState = TransactionEventData::ChargingState::SuspendedEVSE;
        } else if (evReadyInput && !evReadyInput()) {
            chargingState = TransactionEventData::ChargingState::SuspendedEV;
        } else {
            chargingState = TransactionEventData::ChargingState::Charging;
        }
    }

    // tx should be started?
    if (txService.isTxStartPoint(TxStartStopPoint::EVConnected) &&
                connectorPluggedInput && connectorPluggedInput()) {
        txStarted = true;
        triggerReason = TransactionEventTriggerReason::CablePluggedIn;
    } else if (txService.isTxStartPoint(TxStartStopPoint::PowerPathClosed) &&
                evReadyInput && evReadyInput()) {
        txStarted = true;
        triggerReason = TransactionEventTriggerReason::ChargingStateChanged;
    } else if (txService.isTxStartPoint(TxStartStopPoint::Authorized) &&
                transaction && transaction->isAuthorized) {
        txStarted = true;
        triggerReason = TransactionEventTriggerReason::Authorized;
    }

    // tx should be stopped?
    if (transaction && !transaction->active) {
        // tx ended via endTransaction
        txStopped = true;
        triggerReason = transaction->stopTrigger;
        stoppedReason = transaction->stopReason;
    } else if (txService.isTxStopPoint(TxStartStopPoint::EVConnected) &&
                connectorPluggedInput && !connectorPluggedInput() &&
                (txService.stopTxOnEVSideDisconnectBool->getBool() || !transaction || !transaction->started)) {
        txStopped = true;
        triggerReason = TransactionEventTriggerReason::EVCommunicationLost;
        stoppedReason = Ocpp201::Transaction::StopReason::EVDisconnected;
    } else if (txService.isTxStopPoint(TxStartStopPoint::PowerPathClosed) &&
                evReadyInput && !evReadyInput()) {
        txStopped = true;
        triggerReason = TransactionEventTriggerReason::ChargingStateChanged;
        stoppedReason = Ocpp201::Transaction::StopReason::StoppedByEV;
    } else if (txService.isTxStopPoint(TxStartStopPoint::Authorized) &&
                (!transaction || !transaction->isAuthorized)) {
        // user revoked authorization (or EV or any "local" entity)
        txStopped = true;
        triggerReason = TransactionEventTriggerReason::StopAuthorized;
        stoppedReason = Ocpp201::Transaction::StopReason::Local;
    } else if (txService.isTxStopPoint(TxStartStopPoint::Authorized) &&
                transaction && transaction->isDeauthorized &&
                txService.stopTxOnInvalidIdBool->getBool()) {
        // OCPP server rejected authorization
        txStopped = true;
        triggerReason = TransactionEventTriggerReason::Deauthorized;
        stoppedReason = Ocpp201::Transaction::StopReason::DeAuthorized;
    }

    std::shared_ptr<TransactionEventData> txEvent;

    if (txStopped) { // stop tx?
        if (transaction && transaction->started && !transaction->stopped) {
            // yes, stop running tx

            txEvent = std::make_shared<TransactionEventData>(transaction, transaction->seqNoCounter++);
            if (!txEvent) {
                // OOM
                return;
            }

            txEvent->eventType = TransactionEventData::Type::Ended;
            transaction->stopReason = stoppedReason;
        }
    } else {
        if (txStarted && (!transaction || !transaction->started)) { // start tx?
            // yes, start tx

            if (!transaction) {
                transaction = allocateTransaction();
                if (!transaction) {
                    // OOM
                    return;
                }
            }

            txEvent = std::make_shared<TransactionEventData>(transaction, transaction->seqNoCounter++);
            if (!txEvent) {
                // OOM
                return;
            }

            txEvent->eventType = TransactionEventData::Type::Started;
            
        } else if (transaction && transaction->started) {
            // update tx?
            if (chargingState != trackChargingState) {
                txUpdated = true;
                triggerReason = TransactionEventTriggerReason::ChargingStateChanged;
            }
            trackChargingState = chargingState;

            if (transaction) {
                if (connectorPluggedInput && connectorPluggedInput() && !transaction->evConnected.triggered) {
                    transaction->evConnected.triggered = true;
                    txUpdated = true;
                    triggerReason = TransactionEventTriggerReason::CablePluggedIn;
                } else if (connectorPluggedInput && !connectorPluggedInput() && transaction->evConnected.triggered) {
                    transaction->evConnected.triggered = false;
                    txUpdated = true;
                    triggerReason = TransactionEventTriggerReason::EVCommunicationLost;
                } else if (transaction->isAuthorized && !transaction->authorized.triggered) {
                    transaction->authorized.triggered = true;
                    txUpdated = true;
                    triggerReason = TransactionEventTriggerReason::Authorized;
                } else if (!transaction->isAuthorized && transaction->authorized.triggered) {
                    transaction->authorized.triggered = false;
                    txUpdated = true;
                    triggerReason = TransactionEventTriggerReason::StopAuthorized;
                } else if (evReadyInput && evReadyInput() && !transaction->powerPathClosed.triggered) {
                    transaction->powerPathClosed.triggered = true;
                } else if (evReadyInput && !evReadyInput() && transaction->powerPathClosed.triggered) {
                    transaction->powerPathClosed.triggered = false;
                }
            }

            if (txUpdated) {
                // yes, updated

                txEvent = std::make_shared<TransactionEventData>(transaction, transaction->seqNoCounter++);
                if (!txEvent) {
                    // OOM
                    return;
                }

                txEvent->eventType = TransactionEventData::Type::Updated;
            }
        }
    }

    if (txEvent) {
        txEvent->timestamp = context.getModel().getClock().now();
        txEvent->triggerReason = triggerReason;
        txEvent->chargingState = chargingState;
        txEvent->evse = evseId;
        // meterValue not supported

        if (!transaction->stopIdTokenTransmitted && transaction->stopIdToken) {
            txEvent->idToken = std::unique_ptr<IdToken>(new IdToken(*transaction->stopIdToken.get()));
        }  else if (!transaction->idTokenTransmitted) {
            txEvent->idToken = std::unique_ptr<IdToken>(new IdToken(transaction->idToken));
        }
    }

    if (txEvent) {
        auto txEventRequest = makeRequest(new Ocpp201::TransactionEvent(context.getModel(), txEvent));
        txEventRequest->setTimeout(0);
        context.initiateRequest(std::move(txEventRequest));

        if (!transaction->stopIdTokenTransmitted && transaction->stopIdToken) {
            transaction->stopIdTokenTransmitted = true;
        }  else if (!transaction->idTokenTransmitted) {
            transaction->idTokenTransmitted = true;
        }

        if (txEvent->eventType == TransactionEventData::Type::Ended) {
            transaction->stopped = true;
            transaction->active = false;
            MO_DBG_DEBUG("drop completed transaction");
            transaction.reset();
        } else if (txEvent->eventType == TransactionEventData::Type::Started) {
            transaction->started = true;
            transaction->evConnected.triggered = connectorPluggedInput && connectorPluggedInput();
            transaction->powerPathClosed.triggered = evReadyInput && evReadyInput();
            transaction->authorized.triggered = (transaction && transaction->isAuthorized);
        }
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

bool TransactionService::Evse::beginAuthorization(IdToken idToken) {
    MO_DBG_DEBUG("begin auth: %s", idToken.get());

    MO_DBG_DEBUG("patch idTokenType KeyCode for OCTT");
    idToken = IdToken(idToken.get(), IdToken::Type::KeyCode);

    if (transaction && transaction->idToken.get()) {
        MO_DBG_WARN("tx process still running. Please call endTransaction(...) before");
        return false;
    }

    if (!transaction) {
        transaction = allocateTransaction();
        if (!transaction) {
            MO_DBG_ERR("could not allocate Tx");
            return false;
        }
    }

    transaction->idToken = idToken;
    transaction->beginTimestamp = context.getModel().getClock().now();

    auto tx = transaction;

    auto authorize = makeRequest(new Authorize(context.getModel(), idToken));
    if (!authorize) {
        // OOM
        return false;
    }

    authorize->setOnReceiveConfListener([this, tx] (JsonObject response) {
        if (strcmp(response["idTokenInfo"]["status"] | "_Undefined", "Accepted")) {
            MO_DBG_DEBUG("Authorize rejected (%s), abort tx process", tx->idToken.get());
            tx->isDeauthorized = true;
            return;
        }

        MO_DBG_DEBUG("Authorized transaction process (%s)", tx->idToken.get());
        tx->isAuthorized = true;
        tx->idTokenTransmitted = false;
    });
    authorize->setTimeout(20 * 1000);
    context.initiateRequest(std::move(authorize));
    return true;
}
bool TransactionService::Evse::endAuthorization(IdToken idToken) {

    if (!transaction || !transaction->active) {
        //transaction already ended / not active anymore
        return false;
    }

    MO_DBG_DEBUG("End session started by idTag %s",
                            transaction->idToken.get());
    
    if (transaction->idToken.equals(idToken)) {
        // use same idToken like tx start
        transaction->isAuthorized = false;
        transaction->idTokenTransmitted = false;
    } else {
        // use a different idToken for stopping the tx

        auto tx = transaction;

        auto authorize = makeRequest(new Authorize(context.getModel(), idToken));
        if (!authorize) {
            // OOM
            abortTransaction();
            return false;
        }

        authorize->setOnReceiveConfListener([tx, idToken] (JsonObject response) {
            if (strcmp(response["idTokenInfo"]["status"] | "_Undefined", "Accepted")) {
                MO_DBG_DEBUG("Authorize rejected (%s), don't stop tx", idToken.get());
                return;
            }

            MO_DBG_DEBUG("Authorized transaction stop (%s)", idToken.get());

            tx->stopIdToken = std::unique_ptr<IdToken>(new IdToken(idToken));
            if (!tx->stopIdToken) {
                // OOM
                if (tx->active) {
                    tx->active = false;
                    tx->stopTrigger = TransactionEventTriggerReason::AbnormalCondition;
                    tx->stopReason = Ocpp201::Transaction::StopReason::Other;
                }
                return;
            }

            tx->isAuthorized = false;
            tx->stopIdTokenTransmitted = false;
        });
        authorize->setTimeout(20 * 1000);
        context.initiateRequest(std::move(authorize));
    }

    return true;
}
bool TransactionService::Evse::abortTransaction(Ocpp201::Transaction::StopReason stopReason, TransactionEventTriggerReason stopTrigger) {

    if (!transaction || !transaction->active) {
        //transaction already ended / not active anymore
        return false;
    }

    MO_DBG_DEBUG("Abort session started by idTag %s",
                            transaction->idToken.get());

    transaction->active = false;
    transaction->stopTrigger = stopTrigger;
    transaction->stopReason = stopReason;

    return true;
}
const std::shared_ptr<MicroOcpp::Ocpp201::Transaction>& TransactionService::Evse::getTransaction() {
    return transaction;
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

bool TransactionService::parseTxStartStopPoint(const char *csl, std::vector<TxStartStopPoint>& dst) {
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

TransactionService::TransactionService(Context& context) : context(context) {
    auto variableService = context.getModel().getVariableService();

    txStartPointString = variableService->declareVariable<const char*>("TxCtrlr", "TxStartPoint", "PowerPathClosed");
    txStopPointString  = variableService->declareVariable<const char*>("TxCtrlr", "TxStopPoint",  "EVConnected");
    stopTxOnInvalidIdBool = variableService->declareVariable<bool>("TxCtrlr", "StopTxOnInvalidId", true);
    stopTxOnEVSideDisconnectBool = variableService->declareVariable<bool>("TxCtrlr", "StopTxOnEVSideDisconnect", true);
    evConnectionTimeOutInt = variableService->declareVariable<int>("TxCtrlr", "EVConnectionTimeOut", 30);

    variableService->registerValidator<const char*>("TxCtrlr", "TxStartPoint", [this] (const char *value) -> bool {
        std::vector<TxStartStopPoint> validated;
        return this->parseTxStartStopPoint(value, validated);
    });

    variableService->registerValidator<const char*>("TxCtrlr", "TxStopPoint", [this] (const char *value) -> bool {
        std::vector<TxStartStopPoint> validated;
        return this->parseTxStartStopPoint(value, validated);
    });

    for (unsigned int evseId = 1; evseId < MO_NUM_EVSE; evseId++) {
        evses.emplace_back(context, *this, evseId);
    }
}

void TransactionService::loop() {
    for (Evse& evse : evses) {
        evse.loop();
    }

    if (txStartPointString->getWriteCount() != trackTxStartPoint) {
        parseTxStartStopPoint(txStartPointString->getString(), txStartPointParsed);
    }

    if (txStopPointString->getWriteCount() != trackTxStopPoint) {
        parseTxStartStopPoint(txStopPointString->getString(), txStopPointParsed);
    }
}

TransactionService::Evse *TransactionService::getEvse(unsigned int evseId) {
    if (evseId >= 1 && evseId - 1 < evses.size()) {
        return &evses[evseId - 1];
    } else {
        MO_DBG_ERR("invalid arg");
        return nullptr;
    }
}

#endif // MO_ENABLE_V201
