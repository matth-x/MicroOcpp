// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Tasks/ChargePointStatus/ConnectorStatus.h>

#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Core/Configuration.h>

#include <ArduinoOcpp/MessagesV16/StatusNotification.h>
#include <ArduinoOcpp/MessagesV16/StartTransaction.h>
#include <ArduinoOcpp/MessagesV16/StopTransaction.h>
#include <ArduinoOcpp/MessagesV16/CiStrings.h>

#include <ArduinoOcpp/Debug.h>

using namespace ArduinoOcpp;
using namespace ArduinoOcpp::Ocpp16;

ConnectorStatus::ConnectorStatus(OcppModel& context, int connectorId)
        : context(context), connectorId{connectorId} {

    //Set default transaction ID in memory
    char key [CONF_KEYLEN_MAX + 1] = {'\0'};

    snprintf(key, CONF_KEYLEN_MAX + 1, "AO_SID_CONN_%d", connectorId);
    sIdTag = declareConfiguration<const char *>(key, "", CONFIGURATION_FN, false, false, true, false);

    snprintf(key, CONF_KEYLEN_MAX + 1, "AO_TXID_CONN_%d", connectorId);
    transactionId = declareConfiguration<int>(key, -1, CONFIGURATION_FN, false, false, true, false);

    snprintf(key, CONF_KEYLEN_MAX + 1, "AO_AVAIL_CONN_%d", connectorId);
    availability = declareConfiguration<int>(key, AVAILABILITY_OPERATIVE, CONFIGURATION_FN, false, false, true, false);

    connectionTimeOut = declareConfiguration<int>("ConnectionTimeOut", 30, CONFIGURATION_FN, true, true, true, false);
    minimumStatusDuration = declareConfiguration<int>("MinimumStatusDuration", 0, CONFIGURATION_FN, true, true, true, false);
    stopTransactionOnInvalidId = declareConfiguration<const char*>("StopTransactionOnInvalidId", "true", CONFIGURATION_FN, true, true, false, false);
    stopTransactionOnEVSideDisconnect = declareConfiguration<const char*>("StopTransactionOnEVSideDisconnect", "true", CONFIGURATION_FN, true, true, false, false);
    unlockConnectorOnEVSideDisconnect = declareConfiguration<const char*>("UnlockConnectorOnEVSideDisconnect", "true", CONFIGURATION_FN, true, true, false, false);
    localAuthorizeOffline = declareConfiguration<const char*>("LocalAuthorizeOffline", "false", CONFIGURATION_FN, true, true, false, false);
    localPreAuthorize = declareConfiguration<const char*>("LocalPreAuthorize", "false", CONFIGURATION_FN, true, true, false, false);

    if (!sIdTag || !transactionId || !availability) {
        AO_DBG_ERR("Cannot declare sessionIdTag, transactionId or availability");
    }
    if (sIdTag->getBuffsize() > 0 && (*sIdTag)[0] != '\0') {
        snprintf(idTag, min((size_t) (IDTAG_LEN_MAX + 1), sIdTag->getBuffsize()), "%s", ((const char *) *sIdTag));
        session = true;
        connectionTimeOutTimestamp = ao_tick_ms();
        connectionTimeOutListen = true;
        AO_DBG_DEBUG("Load session idTag at initialization");
    }
    transactionIdSync = *transactionId;

    /*
     * Initialize standard EVSE behavior.
     * By default, transactions are triggered by a valid IdTag (+ connected plug as soon as set)
     * The default necessary steps before starting a transaction are
     *     - lock the connector (if handler is set)
     *     - instruct the OCMF meter to begin a transaction (if OCMF meter handler is set)
     */
    txTriggerConditions.push_back([this] () -> TxCondition {
        return getSessionIdTag() == nullptr ? TxCondition::Inactive : TxCondition::Active;
    });
    txEnableSequence.push_back([this] (TxCondition cond) -> TxEnableState {
        if (onOcmfMeterPollTx) {
            return onOcmfMeterPollTx(cond);
        }
        return cond == TxCondition::Active ? TxEnableState::Active : TxEnableState::Inactive;
    });
    txEnableSequence.push_back([this] (TxCondition cond) -> TxEnableState {
        if (onConnectorLockPollTx) {
            return onConnectorLockPollTx(cond);
        }
        return cond == TxCondition::Active ? TxEnableState::Active : TxEnableState::Inactive;
    });
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
        } else {
            return OcppEvseState::Available;
        }
    }

    if (getErrorCode() != nullptr) {
        return OcppEvseState::Faulted;
    } else if (*availability == AVAILABILITY_INOPERATIVE) {
        return OcppEvseState::Unavailable;
    } else if (getTransactionId() == 0 &&      // i.e. Tx pending or EVSE offline. Check if offline Tx is OFF
            !(*localAuthorizeOffline && strcmp(*localAuthorizeOffline, "false")) &&
            !(*localPreAuthorize && strcmp(*localPreAuthorize, "false"))) {
        //All modes for offline Tx are off
        return OcppEvseState::Preparing; //see other Preparing case
    } else if (getTransactionId() >= 0) {
        //Transaction is currently running
        if ((connectorEnergizedSampler && !connectorEnergizedSampler()) ||
                idTagInvalidated) {
            return OcppEvseState::SuspendedEVSE;
        }
        if (evRequestsEnergySampler && !evRequestsEnergySampler()) {
            return OcppEvseState::SuspendedEV;
        }
        return OcppEvseState::Charging;
    } else if (txEnable == TxEnableState::Inactive) {
        return OcppEvseState::Available;
    } else if (txEnable == TxEnableState::Pending ||  
               txEnable == TxEnableState::Active) {   //reached if Tx init is delayed

        if (txEnable == TxEnableState::Active) { // TODO verify if actually possible
            AO_DBG_VERBOSE("Infered Active"); //
            (void)0;                             //
        }                                        //

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

    if (idTagInvalidated) {
        return false;
    }

    OcppEvseState state = inferenceStatus();

    return state == OcppEvseState::Charging ||
            state == OcppEvseState::SuspendedEV ||
            state == OcppEvseState::SuspendedEVSE;
}

OcppMessage *ConnectorStatus::loop() {
    if (getTransactionId() < 0 && *availability == AVAILABILITY_INOPERATIVE_SCHEDULED) {
        *availability = AVAILABILITY_INOPERATIVE;
        saveState();
    }
        
    if (connectorPluggedSampler) {
        if (getTransactionId() >= 0 && !connectorPluggedSampler()) {
            if (!*stopTransactionOnEVSideDisconnect || strcmp(*stopTransactionOnEVSideDisconnect, "false")) {
                endSession("EVDisconnected");
            }
        }
    }

    auto txTrigger = txTriggerConditions.empty() ? TxCondition::Inactive : TxCondition::Active;
    txEnable = TxEnableState::Inactive;

    if (*availability == AVAILABILITY_INOPERATIVE) {
        txTrigger = TxCondition::Inactive;
    }

    if (txTrigger == TxCondition::Active) {
        for (auto trigger = txTriggerConditions.begin(); trigger != txTriggerConditions.end(); trigger++) {
            auto result = trigger->operator()();
            if (result == TxCondition::Active) {
                txEnable = TxEnableState::Pending;
            } else {
                txTrigger = TxCondition::Inactive;
            }
        }
    }

    if (txTrigger == TxCondition::Active) {
        txEnable = TxEnableState::Active;

        for (auto step = txEnableSequence.rbegin(); step != txEnableSequence.rend(); step++) {
            auto result = step->operator()(TxCondition::Active);
            if (result != TxEnableState::Active) {
                txEnable = TxEnableState::Pending;
                break;
            }
        }
    } else {
        for (auto step = txEnableSequence.begin(); step != txEnableSequence.end(); step++) {
            auto result = step->operator()(TxCondition::Inactive);
            if (result != TxEnableState::Inactive) {
                txEnable = TxEnableState::Pending;
                break;
            }
        }
    }


    /*
     * Check conditions for start or stop transaction
     */
    if (txEnable == TxEnableState::Active) {
        //check if not in transaction yet
        if (getTransactionId() < 0 &&
                !getErrorCode()) {
            //start Transaction

            AO_DBG_DEBUG("Session mngt: txId=%i, connectorPlugged = %s, session=%d",
                    getTransactionId(),
                    connectorPluggedSampler ? (connectorPluggedSampler() ? "plugged" : "unplugged") : "undefined",
                    session);
            AO_DBG_INFO("Session mngt: trigger StartTransaction");
            return new StartTransaction(connectorId);
        }
    } else {
        //check if still in transaction
        if (getTransactionId() >= 0) {
            //stop transaction

            AO_DBG_DEBUG("Session mngt: txId=%i, connectorPlugged = %s, session=%d",
                    getTransactionId(),
                    connectorPluggedSampler ? (connectorPluggedSampler() ? "plugged" : "unplugged") : "undefined",
                    session);
            AO_DBG_INFO("Session mngt: trigger StopTransaction");
            return new StopTransaction(connectorId, endReason[0] != '\0' ? endReason : nullptr);
        }
    }

    if (connectionTimeOutListen) {
        if (getTransactionId() >= 0 || !session) {
            AO_DBG_DEBUG("Session mngt: release connectionTimeOut");
            connectionTimeOutListen = false;
        } else {
            if (ao_tick_ms() - connectionTimeOutTimestamp >= ((ulong) *connectionTimeOut) * 1000UL) {
                AO_DBG_INFO("Session mngt: timeout");
                endSession();
                connectionTimeOutListen = false;
            }
        }
    }

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
    AO_DBG_DEBUG("Begin session with idTag %s, overwriting idTag %s", sessionIdTag, idTag);
    if (!sessionIdTag || *sessionIdTag == '\0') {
        //input string is empty
        snprintf(idTag, IDTAG_LEN_MAX + 1, "A0-00-00-00");
    } else {
        snprintf(idTag, IDTAG_LEN_MAX + 1, "%s", sessionIdTag);
    }
    sIdTag->setValue(idTag, IDTAG_LEN_MAX + 1);
    saveState();
    session = true;
    idTagInvalidated = false;

    memset(endReason, '\0', REASON_LEN_MAX + 1);

    connectionTimeOutListen = true;
    connectionTimeOutTimestamp = ao_tick_ms();
}

void ConnectorStatus::endSession(const char *reason) {
    AO_DBG_DEBUG("End session with idTag %s for reason %s, %s previous reason",
                            idTag, reason ? reason : "undefined",
                            endReason[0] == '\0' ? "no" : "overruled by");
    if (session) {
        memset(idTag, '\0', IDTAG_LEN_MAX + 1);
        *sIdTag = "";
        saveState();
    }
    session = false;

    if (reason && endReason[0] == '\0') {
        snprintf(endReason, REASON_LEN_MAX + 1, "%s", reason);
    }

    connectionTimeOutListen = false;
}

void ConnectorStatus::setIdTagInvalidated() {
    if (session) {
        idTagInvalidated = true;
        if (!*stopTransactionOnInvalidId || strcmp(*stopTransactionOnInvalidId, "false")) {
            endSession("DeAuthorized");
        }
    } else {
        AO_DBG_WARN("Cannot invalidate IdTag outside of session");
    }
}

const char *ConnectorStatus::getSessionIdTag() {
    return session ? idTag : nullptr;
}

int ConnectorStatus::getTransactionId() {
    return *transactionId;
}

int ConnectorStatus::getTransactionIdSync() {
    return transactionIdSync;
}

void ConnectorStatus::setTransactionIdSync(int id) {
    transactionIdSync = id;
}

uint16_t ConnectorStatus::getTransactionWriteCount() {
    return transactionId->getValueRevision();
}

void ConnectorStatus::setTransactionId(int id) {
    int prevTxId = *transactionId;
    *transactionId = id;
    if (id != 0 || prevTxId > 0)
        saveState();
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

void ConnectorStatus::setConnectorPluggedSampler(std::function<bool()> connectorPlugged) {
    this->connectorPluggedSampler = connectorPlugged;
    txTriggerConditions.push_back([this] () -> TxCondition {
        return connectorPluggedSampler() ? TxCondition::Active : TxCondition::Inactive;
    });
}

void ConnectorStatus::setEvRequestsEnergySampler(std::function<bool()> evRequestsEnergy) {
    this->evRequestsEnergySampler = evRequestsEnergy;
}

void ConnectorStatus::setConnectorEnergizedSampler(std::function<bool()> connectorEnergized) {
    this->connectorEnergizedSampler = connectorEnergizedSampler;
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

void ConnectorStatus::setConnectorLock(std::function<TxEnableState(TxCondition)> onConnectorLockPollTx) {
    this->onConnectorLockPollTx = onConnectorLockPollTx;
}

void ConnectorStatus::setTxBasedMeterUpdate(std::function<TxEnableState(TxCondition)> onOcmfMeterPollTx) {
    this->onOcmfMeterPollTx = onOcmfMeterPollTx;
}
