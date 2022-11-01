// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Tasks/ChargePointStatus/ConnectorStatus.h>

#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Tasks/Transactions/TransactionStore.h>
#include <ArduinoOcpp/Core/Configuration.h>

#include <ArduinoOcpp/MessagesV16/StatusNotification.h>
#include <ArduinoOcpp/MessagesV16/StartTransaction.h>
#include <ArduinoOcpp/MessagesV16/StopTransaction.h>
#include <ArduinoOcpp/MessagesV16/CiStrings.h>

#include <ArduinoOcpp/Debug.h>

#include <ArduinoOcpp/Tasks/Metering/MeteringService.h>

using namespace ArduinoOcpp;
using namespace ArduinoOcpp::Ocpp16;

ConnectorStatus::ConnectorStatus(OcppModel& context, int connectorId)
        : context(context), connectorId{connectorId}, txProcess(connectorId) {

    char availabilityKey [CONF_KEYLEN_MAX + 1] = {'\0'};
    snprintf(availabilityKey, CONF_KEYLEN_MAX + 1, "AO_AVAIL_CONN_%d", connectorId);
    availability = declareConfiguration<int>(availabilityKey, AVAILABILITY_OPERATIVE, CONFIGURATION_FN, false, false, true, false);

    connectionTimeOut = declareConfiguration<int>("ConnectionTimeOut", 30, CONFIGURATION_FN, true, true, true, false);
    minimumStatusDuration = declareConfiguration<int>("MinimumStatusDuration", 0, CONFIGURATION_FN, true, true, true, false);
    stopTransactionOnInvalidId = declareConfiguration<const char*>("StopTransactionOnInvalidId", "true", CONFIGURATION_FN, true, true, false, false);
    stopTransactionOnEVSideDisconnect = declareConfiguration<const char*>("StopTransactionOnEVSideDisconnect", "true", CONFIGURATION_FN, true, true, false, false);
    unlockConnectorOnEVSideDisconnect = declareConfiguration<const char*>("UnlockConnectorOnEVSideDisconnect", "true", CONFIGURATION_FN, true, true, false, false);
    localAuthorizeOffline = declareConfiguration<const char*>("LocalAuthorizeOffline", "false", CONFIGURATION_FN, true, true, false, false);
    localPreAuthorize = declareConfiguration<const char*>("LocalPreAuthorize", "false", CONFIGURATION_FN, true, true, false, false);

    if (!availability) {
        AO_DBG_ERR("Cannot declare availability");
    }

    /*
     * Initialize standard EVSE behavior.
     * By default, transactions are triggered by a valid IdTag (+ connected plug as soon as set)
     * The default necessary steps before starting a transaction are
     *     - lock the connector (if handler is set)
     *     - instruct the tx-based meter to begin a transaction (if tx-based meter handler is set)
     */
    txProcess.addTrigger([this] () -> TxTrigger {
        if (transaction && transaction->isInSession() && transaction->isActive()) {
            return TxTrigger::Active;
        } else {
            return TxTrigger::Inactive;
        }
    });
    txProcess.addEnableStep([this] (TxTrigger cond) -> TxEnableState {
        if (onTxBasedMeterPollTx) {
            return onTxBasedMeterPollTx(cond);
        }
        return cond == TxTrigger::Active ? TxEnableState::Active : TxEnableState::Inactive;
    });
    txProcess.addEnableStep([this] (TxTrigger cond) -> TxEnableState {
        if (onConnectorLockPollTx) {
            return onConnectorLockPollTx(cond);
        }
        return cond == TxTrigger::Active ? TxEnableState::Active : TxEnableState::Inactive;
    });

    if (context.getTransactionStore()) {
        transaction = context.getTransactionStore()->getLatestTransaction(connectorId);
    } else {
        AO_DBG_ERR("must initialize TxStore before ConnectorStatus");
        (void)0;
    }
}

OcppEvseState ConnectorStatus::inferenceStatus() {
    /*
    * Handle special case: This is the ConnectorStatus for the whole CP (i.e. connectorId=0) --> only states Available, Unavailable, Faulted are possible
    */
    if (connectorId == 0) {
        if (getErrorCode() != nullptr) {
            return OcppEvseState::Faulted;
        } else if (*availability == AVAILABILITY_INOPERATIVE) {
            return OcppEvseState::Unavailable;
        } else if (rebooting) {
            return OcppEvseState::Unavailable;
        } else {
            return OcppEvseState::Available;
        }
    }

    if (getErrorCode() != nullptr) {
        return OcppEvseState::Faulted;
    } else if (*availability == AVAILABILITY_INOPERATIVE) {
        return OcppEvseState::Unavailable;
    } else if (rebooting && (!transaction || !transaction->isRunning())) {
        return OcppEvseState::Unavailable;
    } else if (transaction && transaction->isRunning()) {
        //Transaction is currently running
        if ((connectorEnergizedSampler && !connectorEnergizedSampler()) ||
                !transaction->isActive() || //will forbid charging
                transaction->isIdTagDeauthorized()) {
            return OcppEvseState::SuspendedEVSE;
        }
        if (evRequestsEnergySampler && !evRequestsEnergySampler()) {
            return OcppEvseState::SuspendedEV;
        }
        return OcppEvseState::Charging;
    } else if (!txProcess.existsActiveTrigger() && txProcess.getState() == TxEnableState::Inactive) {
        return OcppEvseState::Available;
    } else {
        /*
         * Either in Preparing or Finishing state. Only way to know is from previous state
         */
        const auto previous = currentStatus;
        if (previous == OcppEvseState::Finishing ||
                previous == OcppEvseState::Charging ||
                previous == OcppEvseState::SuspendedEV ||
                previous == OcppEvseState::SuspendedEVSE) {
            return OcppEvseState::Finishing;
        } else {
            return OcppEvseState::Preparing;
        }
    }

    AO_DBG_VERBOSE("Cannot infere status");
    return OcppEvseState::Faulted; //internal error
}

bool ConnectorStatus::ocppPermitsCharge() {
    if (connectorId == 0) {
        AO_DBG_WARN("not supported for connectorId == 0");
        return false;
    }

    return transaction &&
            transaction->isRunning() &&
            transaction->isActive() &&
            !getErrorCode() &&
            !transaction->isIdTagDeauthorized();
}

OcppMessage *ConnectorStatus::loop() {

    if ((!transaction || !transaction->isRunning()) && *availability == AVAILABILITY_INOPERATIVE_SCHEDULED) {
        *availability = AVAILABILITY_INOPERATIVE;
        saveState();
    }

    if (transaction && (transaction->isAborted() || transaction->isCompleted())) {
        if (transaction->isAborted()) {
            //If the transaction is aborted (invalidated before started), delete all artifacts from flash
            //This is an optimization. The memory management will attempt to remove those files again later
            AO_DBG_DEBUG("collect obsolete transaction %u-%u", connectorId, transaction->getTxNr());
            if (auto mService = context.getMeteringService()) {
                mService->removeTxMeterData(connectorId, transaction->getTxNr());
            }

            context.getTransactionStore()->remove(connectorId, transaction->getTxNr());
        }

        transaction = nullptr;
    }

    txProcess.evaluateProcessSteps();

    if (transaction) { //begin exclusively transaction-related operations
            
        if (connectorPluggedSampler) {
            if (transaction->isRunning() && transaction->isInSession() && !connectorPluggedSampler()) {
                if (!*stopTransactionOnEVSideDisconnect || strcmp(*stopTransactionOnEVSideDisconnect, "false")) {
                    AO_DBG_DEBUG("Stop Tx due to EV disconnect");
                    transaction->setStopReason("EVDisconnected");
                    transaction->endSession();
                    transaction->commit();
                }
            }
        }

        if (transaction->isInSession() &&
                !transaction->getStartRpcSync().isRequested() &&
                transaction->getSessionTimestamp() > MIN_TIME &&
                connectionTimeOut && *connectionTimeOut > 0 &&
                context.getOcppTime().getOcppTimestampNow() - transaction->getSessionTimestamp() >= (otime_t) *connectionTimeOut) {
                
            AO_DBG_INFO("Session mngt: timeout");
            transaction->endSession();
            transaction->commit();
        }

        if (transaction->isInSession() && transaction->isIdTagDeauthorized()) {
            if (!*stopTransactionOnInvalidId || strcmp(*stopTransactionOnInvalidId, "false")) {
                AO_DBG_DEBUG("DeAuthorize session");
                transaction->setStopReason("DeAuthorized");
                transaction->endSession();
                transaction->commit();
            }
        }

        auto txEnable = txProcess.getState();

        /*
        * Check conditions for start or stop transaction
        */

        if (txEnable == TxEnableState::Active) {
            
            if (transaction->isPreparing() && !getErrorCode()) {
                //start Transaction

                AO_DBG_INFO("Session mngt: trigger StartTransaction");
                auto meteringService = context.getMeteringService();
                if (transaction->getMeterStart() < 0 && meteringService) {
                    auto meterStart = meteringService->readTxEnergyMeter(transaction->getConnectorId(), ReadingContext::TransactionBegin);
                    if (meterStart && *meterStart) {
                        transaction->setMeterStart(meterStart->toInteger());
                    } else {
                        AO_DBG_ERR("MeterStart undefined");
                    }
                }

                if (transaction->getStartTimestamp() <= MIN_TIME) {
                    transaction->setStartTimestamp(context.getOcppTime().getOcppTimestampNow());
                }

                transaction->commit();

                if (context.getMeteringService()) {
                    context.getMeteringService()->beginTxMeterData(transaction.get());
                }

                return new StartTransaction(transaction);
            }
        } else if (transaction->isRunning()) {

            if (transaction->isInSession()) {
                AO_DBG_DEBUG("Tx process not active");
                transaction->endSession();
                transaction->commit();
            }

            if (txEnable == TxEnableState::Inactive) {
                //stop transaction

                AO_DBG_INFO("Session mngt: trigger StopTransaction");
                
                auto meteringService = context.getMeteringService();
                if (transaction->getMeterStop() < 0 && meteringService) {
                    auto meterStop = meteringService->readTxEnergyMeter(transaction->getConnectorId(), ReadingContext::TransactionEnd);
                    if (meterStop && *meterStop) {
                        transaction->setMeterStop(meterStop->toInteger());
                    } else {
                        AO_DBG_ERR("MeterStop undefined");
                    }
                }

                if (transaction->getStopTimestamp() <= MIN_TIME) {
                    transaction->setStopTimestamp(context.getOcppTime().getOcppTimestampNow());
                }

                transaction->commit();

                std::shared_ptr<TransactionMeterData> stopTxData;

                if (meteringService) {
                    stopTxData = meteringService->endTxMeterData(transaction.get());
                }

                if (stopTxData) {
                    return new StopTransaction(std::move(transaction), stopTxData->retrieveStopTxData());
                } else {
                    return new StopTransaction(std::move(transaction));
                }
            }
        }
    } //end transaction-related operations

    auto inferedStatus = inferenceStatus();
    
    if (inferedStatus != currentStatus) {
        currentStatus = inferedStatus;
        t_statusTransition = ao_tick_ms();
        AO_DBG_DEBUG("Status changed%s", *minimumStatusDuration ? ", will report delayed" : "");
    }

    if (reportedStatus != currentStatus &&
            (*minimumStatusDuration <= 0 || //MinimumStatusDuration disabled
            ao_tick_ms() - t_statusTransition >= ((ulong) *minimumStatusDuration) * 1000UL)) {
        reportedStatus = currentStatus;
        OcppTimestamp reportedTimestamp = context.getOcppTime().getOcppTimestampNow();
        reportedTimestamp -= (ao_tick_ms() - t_statusTransition) / 1000UL;

        return new StatusNotification(connectorId, reportedStatus, reportedTimestamp, getErrorCode());
    }

    return nullptr;
}

const char *ConnectorStatus::getErrorCode() {
    for (auto s = connectorErrorCodeSamplers.begin(); s != connectorErrorCodeSamplers.end(); s++) {
        const char *err = s->operator()();
        if (err != nullptr) {
            return err;
        }
    }
    return nullptr;
}

void ConnectorStatus::beginSession(const char *sessionIdTag) {
    
    if (!transaction) {
        transaction = context.getTransactionStore()->createTransaction(connectorId);

        if (!transaction) {
            AO_DBG_ERR("Could not allocate Tx");
            return;
        }
    }
    
    AO_DBG_DEBUG("Begin session with idTag %s, overwriting idTag %s", sessionIdTag != nullptr ? sessionIdTag : "", transaction->getIdTag());
    if (!sessionIdTag || *sessionIdTag == '\0') {
        //input string is empty
        transaction->setIdTag("A0-00-00-00");
    } else {
        transaction->setIdTag(sessionIdTag);
    }

    transaction->setSessionTimestamp(context.getOcppTime().getOcppTimestampNow());

    transaction->commit();
}

void ConnectorStatus::endSession(const char *reason) {

    if (!transaction) {
        AO_DBG_WARN("Cannot end session if no transaction running");
        return;
    }

    if (transaction->isInSession()) {
        AO_DBG_DEBUG("End session with idTag %s for reason %s, %s previous reason",
                                transaction->getIdTag(), reason ? reason : "undefined",
                                *transaction->getStopReason() ? "overruled by" : "no");

        if (reason) {
            transaction->setStopReason(reason);
        }
        transaction->endSession();
        transaction->commit();
    }
}

const char *ConnectorStatus::getSessionIdTag() {

    if (!transaction) {
        return nullptr;
    }

    return transaction->isInSession() ? transaction->getIdTag() : nullptr;
}

uint16_t ConnectorStatus::getSessionWriteCount() {
    return transaction ? transaction->getTxNr() : 0;
}

bool ConnectorStatus::isTransactionRunning() {
    if (!transaction) {
        return false;
    }

    return transaction->isRunning();
}

int ConnectorStatus::getTransactionId() {
    
    if (!transaction) {
        return -1;
    }

    if (transaction->isRunning()) {
        if (transaction->getStartRpcSync().isConfirmed()) {
            return transaction->getTransactionId();
        } else {
            return 0;
        }
    } else {
        return -1;
    }
}

std::shared_ptr<Transaction>& ConnectorStatus::getTransaction() {
    return transaction;
}

int ConnectorStatus::getAvailability() {
    return *availability;
}

void ConnectorStatus::setAvailability(bool available) {
    if (available) {
        *availability = AVAILABILITY_OPERATIVE;
    } else {
        if (getTransactionId() > 0) {
            *availability = AVAILABILITY_INOPERATIVE_SCHEDULED;
        } else {
            *availability = AVAILABILITY_INOPERATIVE;
        }
    }
    saveState();
}

void ConnectorStatus::setRebooting(bool rebooting) {
    this->rebooting = rebooting;
}

void ConnectorStatus::setConnectorPluggedSampler(std::function<bool()> connectorPlugged) {
    this->connectorPluggedSampler = connectorPlugged;
    txProcess.addTrigger([this] () -> TxTrigger {
        return connectorPluggedSampler() ? TxTrigger::Active : TxTrigger::Inactive;
    });
}

void ConnectorStatus::setEvRequestsEnergySampler(std::function<bool()> evRequestsEnergy) {
    this->evRequestsEnergySampler = evRequestsEnergy;
}

void ConnectorStatus::setConnectorEnergizedSampler(std::function<bool()> connectorEnergized) {
    this->connectorEnergizedSampler = connectorEnergized;
}

void ConnectorStatus::addConnectorErrorCodeSampler(std::function<const char *()> connectorErrorCode) {
    this->connectorErrorCodeSamplers.push_back(connectorErrorCode);
}

void ConnectorStatus::saveState() {
    configuration_save();
}

void ConnectorStatus::setOnUnlockConnector(std::function<PollResult<bool>()> unlockConnector) {
    this->onUnlockConnector = unlockConnector;
}

std::function<PollResult<bool>()> ConnectorStatus::getOnUnlockConnector() {
    return this->onUnlockConnector;
}

void ConnectorStatus::setConnectorLock(std::function<TxEnableState(TxTrigger)> onConnectorLockPollTx) {
    this->onConnectorLockPollTx = onConnectorLockPollTx;
}

void ConnectorStatus::setTxBasedMeterUpdate(std::function<TxEnableState(TxTrigger)> onTxBasedMeterPollTx) {
    this->onTxBasedMeterPollTx = onTxBasedMeterPollTx;
}
