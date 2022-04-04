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
    if (!sIdTag || !transactionId || !availability) {
        AO_DBG_ERR("Cannot declare sessionIdTag, transactionId or availability");
    }
    if (sIdTag->getBuffsize() > 0 && (*sIdTag)[0] != '\0') {
        snprintf(idTag, std::min((size_t) (IDTAG_LEN_MAX + 1), sIdTag->getBuffsize()), "%s", ((const char *) *sIdTag));
        session = true;
        connectionTimeOutTimestamp = ao_tick_ms();
        connectionTimeOutListen = true;
        AO_DBG_DEBUG("Load session idTag at initialization");
    }
    transactionIdSync = *transactionId;
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

//    auto cpStatusService = context.getChargePointStatusService();
//
//    if (!authorized && !getChargePointStatusService()->existsUnboundAuthorization()) {
//        return OcppEvseState::Available;
//    } else if (((int) *transactionId) < 0) {
//        return OcppEvseState::Preparing;
    //if (connectorFaultedSampler != nullptr && connectorFaultedSampler()) {
    if (getErrorCode() != nullptr) {
        return OcppEvseState::Faulted;
    } else if (*availability == AVAILABILITY_INOPERATIVE) {
        return OcppEvseState::Unavailable;
    } else if (!session &&
                getTransactionId() < 0 &&
                (connectorPluggedSampler == nullptr || !connectorPluggedSampler()) ) {
        return OcppEvseState::Available;
    } else if (getTransactionId() <= 0) {
        if (connectorPluggedSampler != nullptr && connectorPluggedSampler() &&
                (currentStatus == OcppEvseState::Finishing ||
                currentStatus == OcppEvseState::Charging ||
                currentStatus == OcppEvseState::SuspendedEV ||
                currentStatus == OcppEvseState::SuspendedEVSE)) {
            return OcppEvseState::Finishing;
        }
        return OcppEvseState::Preparing;
    } else {
        //Transaction is currently running
        if (evRequestsEnergySampler && !evRequestsEnergySampler()) {
            return OcppEvseState::SuspendedEV;
        }
        if (connectorEnergizedSampler && !connectorEnergizedSampler()) {
            return OcppEvseState::SuspendedEVSE;
        }
        return OcppEvseState::Charging;
    }
}

bool ConnectorStatus::ocppPermitsCharge() {
    if (connectorId == 0) {
        AO_DBG_WARN("not supported for connectorId == 0");
        return false;
    }

    OcppEvseState state = inferenceStatus();

    return state == OcppEvseState::Charging ||
            state == OcppEvseState::SuspendedEV ||
            state == OcppEvseState::SuspendedEVSE;
}

OcppMessage *ConnectorStatus::loop() {
    if (getTransactionId() <= 0 && *availability == AVAILABILITY_INOPERATIVE_SCHEDULED) {
        *availability = AVAILABILITY_INOPERATIVE;
        saveState();
    }

    /*
     * Check conditions for start or stop transaction
     */
    if (connectorPluggedSampler) { //only supported with connectorPluggedSampler
        if (getTransactionId() >= 0) {
            //check condition for StopTransaction
            if (!connectorPluggedSampler() ||
                    !session) {
                AO_DBG_DEBUG("Session mngt: txId=%i, connectorPlugged=%d, session=%d",
                        getTransactionId(), connectorPluggedSampler(), session);
                AO_DBG_INFO("Session mngt: trigger StopTransaction");
                return new StopTransaction(connectorId);
            }
        } else {
            //check condition for StartTransaction
            if (connectorPluggedSampler() &&
                    session &&
                    !getErrorCode() &&
                    *availability == AVAILABILITY_OPERATIVE) {
                AO_DBG_DEBUG("Session mngt: txId=%i, connectorPlugged=%d, session=%d",
                        getTransactionId(), connectorPluggedSampler(), session);
                AO_DBG_INFO("Session mngt: trigger StartTransaction");
                return new StartTransaction(connectorId);
            }
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

    auto inferencedStatus = inferenceStatus();
    
    if (inferencedStatus != currentStatus) {
        currentStatus = inferencedStatus;
        AO_DBG_DEBUG("Status changed");

        //fire StatusNotification
        //TODO check for online condition: Only inform CS about status change if CP is online
        //TODO check for too short duration condition: Only inform CS about status change if it lasted for longer than MinimumStatusDuration
        return new StatusNotification(connectorId, currentStatus, context.getOcppTime().getOcppTimestampNow(), getErrorCode());
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

    connectionTimeOutListen = true;
    connectionTimeOutTimestamp = ao_tick_ms();
}

void ConnectorStatus::endSession() {
    AO_DBG_DEBUG("End session with idTag %s", idTag);
    if (session) {
        memset(idTag, '\0', IDTAG_LEN_MAX + 1);
        *sIdTag = "";
        saveState();
    }
    session = false;

    connectionTimeOutListen = false;
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

void ConnectorStatus::setOnUnlockConnector(std::function<bool()> unlockConnector) {
    this->onUnlockConnector = unlockConnector;
}

std::function<bool()> ConnectorStatus::getOnUnlockConnector() {
    return this->onUnlockConnector;
}
