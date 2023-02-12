// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
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
#include <ArduinoOcpp/Tasks/Reservation/ReservationService.h>

using namespace ArduinoOcpp;
using namespace ArduinoOcpp::Ocpp16;

ConnectorStatus::ConnectorStatus(OcppModel& context, int connectorId)
        : context(context), connectorId{connectorId}, txProcess(connectorId) {

    char availabilityKey [CONF_KEYLEN_MAX + 1] = {'\0'};
    snprintf(availabilityKey, CONF_KEYLEN_MAX + 1, "AO_AVAIL_CONN_%d", connectorId);
    availability = declareConfiguration<int>(availabilityKey, AVAILABILITY_OPERATIVE, CONFIGURATION_FN, false, false, true, false);

    connectionTimeOut = declareConfiguration<int>("ConnectionTimeOut", 30, CONFIGURATION_FN, true, true, true, false);
    minimumStatusDuration = declareConfiguration<int>("MinimumStatusDuration", 0, CONFIGURATION_FN, true, true, true, false);
    stopTransactionOnInvalidId = declareConfiguration<bool>("StopTransactionOnInvalidId", true, CONFIGURATION_FN, true, true, true, false);
    stopTransactionOnEVSideDisconnect = declareConfiguration<bool>("StopTransactionOnEVSideDisconnect", true, CONFIGURATION_FN, true, true, true, false);
    unlockConnectorOnEVSideDisconnect = declareConfiguration<bool>("UnlockConnectorOnEVSideDisconnect", true, CONFIGURATION_FN, true, true, true, false);
    localAuthorizeOffline = declareConfiguration<bool>("LocalAuthorizeOffline", false, CONFIGURATION_FN, true, true, true, false);
    localPreAuthorize = declareConfiguration<bool>("LocalPreAuthorize", false, CONFIGURATION_FN, true, true, true, false);

    //if the EVSE goes offline, can it continue to charge without sending StartTx / StopTx to the server when going online again?
    silentOfflineTransactions = declareConfiguration<bool>("AO_SilentOfflineTransactions", false, CONFIGURATION_FN, true, true, true, false);

    //FreeVend mode
    freeVendActive = declareConfiguration<bool>("AO_FreeVendActive", false, CONFIGURATION_FN, true, true, true, false);
    freeVendIdTag = declareConfiguration<const char*>("AO_FreeVendIdTag", "", CONFIGURATION_FN, true, true, true, false);

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
        } else if (getAvailability() == AVAILABILITY_INOPERATIVE) {
            return OcppEvseState::Unavailable;
        } else {
            return OcppEvseState::Available;
        }
    }

    if (getErrorCode() != nullptr) {
        return OcppEvseState::Faulted;
    } else if (getAvailability() == AVAILABILITY_INOPERATIVE) {
        return OcppEvseState::Unavailable;
    } else if (transaction && transaction->isRunning()) {
        //Transaction is currently running
        if (!ocppPermitsCharge() ||
                (connectorEnergizedSampler && !connectorEnergizedSampler())) {
            return OcppEvseState::SuspendedEVSE;
        }
        if (evRequestsEnergySampler && !evRequestsEnergySampler()) {
            return OcppEvseState::SuspendedEV;
        }
        return OcppEvseState::Charging;
    } else if (context.getReservationService() && context.getReservationService()->getReservation(connectorId)) {
        return OcppEvseState::Reserved;
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

    bool suspendDeAuthorizedIdTag = transaction && transaction->isIdTagDeauthorized(); //if idTag status is "DeAuthorized" and if charging should stop
    
    //check special case for DeAuthorized idTags: FreeVend mode
    if (suspendDeAuthorizedIdTag && freeVendActive && *freeVendActive) {
        suspendDeAuthorizedIdTag = false;
    }

    return transaction &&
            transaction->isRunning() &&
            transaction->isActive() &&
            !getErrorCode() &&
            !suspendDeAuthorizedIdTag;
}

OcppMessage *ConnectorStatus::loop() {

    if (!isTransactionRunning()) {
        if (*availability == AVAILABILITY_INOPERATIVE_SCHEDULED) {
            *availability = AVAILABILITY_INOPERATIVE;
            configuration_save();
        }
        if (availabilityVolatile == AVAILABILITY_INOPERATIVE_SCHEDULED) {
            availabilityVolatile = AVAILABILITY_INOPERATIVE;
        }
    }

    if (transaction && transaction->isAborted()) {
        //If the transaction is aborted (invalidated before started), delete all artifacts from flash
        //This is an optimization. The memory management will attempt to remove those files again later
        bool removed = true;
        if (auto mService = context.getMeteringService()) {
            removed &= mService->removeTxMeterData(connectorId, transaction->getTxNr());
        }

        if (removed) {
            removed &= context.getTransactionStore()->remove(connectorId, transaction->getTxNr());
        }

        if (removed) {
            context.getTransactionStore()->setTxEnd(connectorId, transaction->getTxNr()); //roll back creation of last tx entry
        }

        AO_DBG_DEBUG("collect aborted transaction %u-%u %s", connectorId, transaction->getTxNr(), removed ? "" : "failure");
        transaction = nullptr;
    }

    if (transaction && transaction->isCompleted()) {
        AO_DBG_DEBUG("collect obsolete transaction %u-%u", connectorId, transaction->getTxNr());
        transaction = nullptr;
    }

    txProcess.evaluateProcessSteps();

    if (transaction) { //begin exclusively transaction-related operations
            
        if (connectorPluggedSampler) {
            if (transaction->isRunning() && transaction->isInSession() && !connectorPluggedSampler()) {
                if (!stopTransactionOnEVSideDisconnect || *stopTransactionOnEVSideDisconnect) {
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
            if (!stopTransactionOnInvalidId || *stopTransactionOnInvalidId) {
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

                if (transaction->isSilent()) {
                    AO_DBG_INFO("silent Transaction: omit StartTx");
                    transaction->getStartRpcSync().setRequested();
                    transaction->getStartRpcSync().confirm();
                    transaction->commit();
                    return nullptr;
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

                if (transaction->isSilent()) {
                    AO_DBG_INFO("silent Transaction: omit StopTx");
                    transaction->getStopRpcSync().setRequested();
                    transaction->getStopRpcSync().confirm();
                    if (auto mService = context.getMeteringService()) {
                        mService->removeTxMeterData(connectorId, transaction->getTxNr());
                    }
                    context.getTransactionStore()->remove(connectorId, transaction->getTxNr());
                    context.getTransactionStore()->setTxEnd(connectorId, transaction->getTxNr());
                    transaction = nullptr;
                    return nullptr;
                }
                
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

    //handle FreeVend mode
    if (freeVendActive && *freeVendActive && connectorPluggedSampler) {
        if (!freeVendTrackPlugged && connectorPluggedSampler() && !transaction) {
            const char *idTag = freeVendIdTag && *freeVendIdTag ? *freeVendIdTag : "";
            if (!idTag || *idTag == '\0') {
                idTag = "A0000000";
            }
            AO_DBG_INFO("begin FreeVend Tx using idTag %s", idTag);
            beginSession(idTag);
            
            if (!transaction) {
                AO_DBG_ERR("could not begin FreeVend Tx");
                (void)0;
            }
        }

        freeVendTrackPlugged = connectorPluggedSampler();
    }

    auto inferedStatus = inferenceStatus();
    
    if (inferedStatus != currentStatus) {
        currentStatus = inferedStatus;
        t_statusTransition = ao_tick_ms();
        AO_DBG_DEBUG("Status changed%s", *minimumStatusDuration ? ", will report delayed" : "");
    }

    if (reportedStatus != currentStatus &&
            (*minimumStatusDuration <= 0 || //MinimumStatusDuration disabled
            ao_tick_ms() - t_statusTransition >= ((unsigned long) *minimumStatusDuration) * 1000UL)) {
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
    //(special case: tx already exists -> update the sessionIdTag and good)

    if (!transaction) {
        //no tx running, before creating new one, clean possible aorted tx

        auto txr = context.getTransactionStore()->getTxEnd(connectorId);
        auto txsize = context.getTransactionStore()->size(connectorId);
        for (decltype(txsize) i = 0; i < txsize; i++) {
            txr = (txr + MAX_TX_CNT - 1) % MAX_TX_CNT; //decrement by 1
            
            auto tx = context.getTransactionStore()->getTransaction(connectorId, txr);
            //check if dangling silent tx, aborted tx, or corrupted entry (tx == null)
            if (!tx || tx->isSilent() || tx->isAborted()) {
                //yes, remove
                bool removed = true;
                if (auto mService = context.getMeteringService()) {
                    removed &= mService->removeTxMeterData(connectorId, txr);
                }
                if (removed) {
                    removed &= context.getTransactionStore()->remove(connectorId, txr);
                }
                if (removed) {
                    context.getTransactionStore()->setTxEnd(connectorId, txr);
                    AO_DBG_WARN("deleted dangling silent tx for new transaction");
                } else {
                    AO_DBG_ERR("memory corruption");
                    break;
                }
            } else {
                //no, tx record trimmed, end
                break;
            }
        }

        //try to create new transaction
        transaction = context.getTransactionStore()->createTransaction(connectorId);
    }

    if (!transaction) {
        //could not create transaction - now, try to replace tx history entry

        auto txl = context.getTransactionStore()->getTxBegin(connectorId);
        auto txsize = context.getTransactionStore()->size(connectorId);

        for (decltype(txsize) i = 0; i < txsize; i++) {

            if (transaction) {
                //success, finished here
                break;
            }

            //no transaction allocated, delete history entry to make space

            auto txhist = context.getTransactionStore()->getTransaction(connectorId, txl);
            //oldest entry, now check if it's history and can be removed or corrupted entry
            if (!txhist || txhist->isCompleted()) {
                //yes, remove
                bool removed = true;
                if (auto mService = context.getMeteringService()) {
                    removed &= mService->removeTxMeterData(connectorId, txl);
                }
                if (removed) {
                    removed &= context.getTransactionStore()->remove(connectorId, txl);
                }
                if (removed) {
                    context.getTransactionStore()->setTxBegin(connectorId, (txl + 1) % MAX_TX_CNT);
                    AO_DBG_DEBUG("deleted tx history entry for new transaction");

                    transaction = context.getTransactionStore()->createTransaction(connectorId);
                } else {
                    AO_DBG_ERR("memory corruption");
                    break;
                }
            } else {
                //no, end of history reached, don't delete further tx
                AO_DBG_DEBUG("cannot delete more tx");
                break;
            }

            txl++;
            txl %= MAX_TX_CNT;
        }
    }

    if (!transaction) {
        //couldn't create normal transaction -> check if to start charging without real transaction
        if (silentOfflineTransactions && *silentOfflineTransactions) {
            //try to handle charging session without sending StartTx or StopTx to the server
            transaction = context.getTransactionStore()->createTransaction(connectorId, true);

            if (transaction) {
                AO_DBG_DEBUG("created silent transaction");
                (void)0;
            }
        }
    }

    if (!transaction) {
        AO_DBG_ERR("could not allocate Tx");
        return;
    }
    
    AO_DBG_DEBUG("Begin session with idTag %s, overwriting idTag %s", sessionIdTag != nullptr ? sessionIdTag : "", transaction->getIdTag());
    if (!sessionIdTag || *sessionIdTag == '\0') {
        //input string is empty
        transaction->setIdTag("A0-00-00-00");
    } else {
        transaction->setIdTag(sessionIdTag);
    }

    transaction->setSessionTimestamp(context.getOcppTime().getOcppTimestampNow());

    if (context.getReservationService()) {
        if (auto reservation = context.getReservationService()->getReservation(connectorId, sessionIdTag, nullptr)) { //TODO set parentIdTag
            if (reservation->matches(sessionIdTag, nullptr)) { //TODO set parentIdTag
                transaction->setReservationId(reservation->getReservationId());
            }
        }
    }

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
    if (availabilityVolatile == AVAILABILITY_INOPERATIVE || *availability == AVAILABILITY_INOPERATIVE) {
        return AVAILABILITY_INOPERATIVE;
    } else if (availabilityVolatile == AVAILABILITY_INOPERATIVE_SCHEDULED || *availability == AVAILABILITY_INOPERATIVE_SCHEDULED) {
        return AVAILABILITY_INOPERATIVE_SCHEDULED;
    } else {
        return AVAILABILITY_OPERATIVE;
    }
}

void ConnectorStatus::setAvailability(bool available) {
    if (available) {
        *availability = AVAILABILITY_OPERATIVE;
    } else {
        if (isTransactionRunning()) {
            *availability = AVAILABILITY_INOPERATIVE_SCHEDULED;
        } else {
            *availability = AVAILABILITY_INOPERATIVE;
        }
    }
    configuration_save();
}

void ConnectorStatus::setAvailabilityVolatile(bool available) {
    if (available) {
        availabilityVolatile = AVAILABILITY_OPERATIVE;
    } else {
        if (isTransactionRunning()) {
            availabilityVolatile = AVAILABILITY_INOPERATIVE_SCHEDULED;
        } else {
            availabilityVolatile = AVAILABILITY_INOPERATIVE;
        }
    }
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
