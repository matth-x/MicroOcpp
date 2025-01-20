// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

bool Transaction::setIdTag(const char *idTag) {
    auto ret = snprintf(this->idTag, IDTAG_LEN_MAX + 1, "%s", idTag);
    return ret >= 0 && ret < IDTAG_LEN_MAX + 1;
}

bool Transaction::setParentIdTag(const char *idTag) {
    auto ret = snprintf(this->parentIdTag, IDTAG_LEN_MAX + 1, "%s", idTag);
    return ret >= 0 && ret < IDTAG_LEN_MAX + 1;
}

bool Transaction::setStopIdTag(const char *idTag) {
    auto ret = snprintf(stop_idTag, IDTAG_LEN_MAX + 1, "%s", idTag);
    return ret >= 0 && ret < IDTAG_LEN_MAX + 1;
}

bool Transaction::setStopReason(const char *reason) {
    auto ret = snprintf(stop_reason, REASON_LEN_MAX + 1, "%s", reason);
    return ret >= 0 && ret < REASON_LEN_MAX + 1;
}

bool Transaction::commit() {
    return context.commit(this);
}

#if MO_ENABLE_V201

namespace MicroOcpp {
namespace Ocpp201 {

const char *serializeTransactionStoppedReason(Transaction::StoppedReason stoppedReason) {
    const char *stoppedReasonCstr = nullptr;
    switch (stoppedReason) {
        case Transaction::StoppedReason::UNDEFINED:
            // optional, okay
            break;
        case Transaction::StoppedReason::Local:
            stoppedReasonCstr = "Local";
            break;
        case Transaction::StoppedReason::DeAuthorized:
            stoppedReasonCstr = "DeAuthorized";
            break;
        case Transaction::StoppedReason::EmergencyStop:
            stoppedReasonCstr = "EmergencyStop";
            break;
        case Transaction::StoppedReason::EnergyLimitReached:
            stoppedReasonCstr = "EnergyLimitReached";
            break;
        case Transaction::StoppedReason::EVDisconnected:
            stoppedReasonCstr = "EVDisconnected";
            break;
        case Transaction::StoppedReason::GroundFault:
            stoppedReasonCstr = "GroundFault";
            break;
        case Transaction::StoppedReason::ImmediateReset:
            stoppedReasonCstr = "ImmediateReset";
            break;
        case Transaction::StoppedReason::LocalOutOfCredit:
            stoppedReasonCstr = "LocalOutOfCredit";
            break;
        case Transaction::StoppedReason::MasterPass:
            stoppedReasonCstr = "MasterPass";
            break;
        case Transaction::StoppedReason::Other:
            stoppedReasonCstr = "Other";
            break;
        case Transaction::StoppedReason::OvercurrentFault:
            stoppedReasonCstr = "OvercurrentFault";
            break;
        case Transaction::StoppedReason::PowerLoss:
            stoppedReasonCstr = "PowerLoss";
            break;
        case Transaction::StoppedReason::PowerQuality:
            stoppedReasonCstr = "PowerQuality";
            break;
        case Transaction::StoppedReason::Reboot:
            stoppedReasonCstr = "Reboot";
            break;
        case Transaction::StoppedReason::Remote:
            stoppedReasonCstr = "Remote";
            break;
        case Transaction::StoppedReason::SOCLimitReached:
            stoppedReasonCstr = "SOCLimitReached";
            break;
        case Transaction::StoppedReason::StoppedByEV:
            stoppedReasonCstr = "StoppedByEV";
            break;
        case Transaction::StoppedReason::TimeLimitReached:
            stoppedReasonCstr = "TimeLimitReached";
            break;
        case Transaction::StoppedReason::Timeout:
            stoppedReasonCstr = "Timeout";
            break;
    }

    return stoppedReasonCstr;
}
bool deserializeTransactionStoppedReason(const char *stoppedReasonCstr, Transaction::StoppedReason& stoppedReasonOut) {
    if (!stoppedReasonCstr || !*stoppedReasonCstr) {
        stoppedReasonOut = Transaction::StoppedReason::UNDEFINED;
    } else if (!strcmp(stoppedReasonCstr, "DeAuthorized")) {
        stoppedReasonOut = Transaction::StoppedReason::DeAuthorized;
    } else if (!strcmp(stoppedReasonCstr, "EmergencyStop")) {
        stoppedReasonOut = Transaction::StoppedReason::EmergencyStop;
    } else if (!strcmp(stoppedReasonCstr, "EnergyLimitReached")) {
        stoppedReasonOut = Transaction::StoppedReason::EnergyLimitReached;
    } else if (!strcmp(stoppedReasonCstr, "EVDisconnected")) {
        stoppedReasonOut = Transaction::StoppedReason::EVDisconnected;
    } else if (!strcmp(stoppedReasonCstr, "GroundFault")) {
        stoppedReasonOut = Transaction::StoppedReason::GroundFault;
    } else if (!strcmp(stoppedReasonCstr, "ImmediateReset")) {
        stoppedReasonOut = Transaction::StoppedReason::ImmediateReset;
    } else if (!strcmp(stoppedReasonCstr, "Local")) {
        stoppedReasonOut = Transaction::StoppedReason::Local;
    } else if (!strcmp(stoppedReasonCstr, "LocalOutOfCredit")) {
        stoppedReasonOut = Transaction::StoppedReason::LocalOutOfCredit;
    } else if (!strcmp(stoppedReasonCstr, "MasterPass")) {
        stoppedReasonOut = Transaction::StoppedReason::MasterPass;
    } else if (!strcmp(stoppedReasonCstr, "Other")) {
        stoppedReasonOut = Transaction::StoppedReason::Other;
    } else if (!strcmp(stoppedReasonCstr, "OvercurrentFault")) {
        stoppedReasonOut = Transaction::StoppedReason::OvercurrentFault;
    } else if (!strcmp(stoppedReasonCstr, "PowerLoss")) {
        stoppedReasonOut = Transaction::StoppedReason::PowerLoss;
    } else if (!strcmp(stoppedReasonCstr, "PowerQuality")) {
        stoppedReasonOut = Transaction::StoppedReason::PowerQuality;
    } else if (!strcmp(stoppedReasonCstr, "Reboot")) {
        stoppedReasonOut = Transaction::StoppedReason::Reboot;
    } else if (!strcmp(stoppedReasonCstr, "Remote")) {
        stoppedReasonOut = Transaction::StoppedReason::Remote;
    } else if (!strcmp(stoppedReasonCstr, "SOCLimitReached")) {
        stoppedReasonOut = Transaction::StoppedReason::SOCLimitReached;
    } else if (!strcmp(stoppedReasonCstr, "StoppedByEV")) {
        stoppedReasonOut = Transaction::StoppedReason::StoppedByEV;
    } else if (!strcmp(stoppedReasonCstr, "TimeLimitReached")) {
        stoppedReasonOut = Transaction::StoppedReason::TimeLimitReached;
    } else if (!strcmp(stoppedReasonCstr, "Timeout")) {
        stoppedReasonOut = Transaction::StoppedReason::Timeout;
    } else {
        MO_DBG_ERR("deserialization error");
        return false;
    }
    return true;
}

const char *serializeTransactionEventType(TransactionEventData::Type type) {
    const char *typeCstr = "";
    switch (type) {
        case TransactionEventData::Type::Ended:
            typeCstr = "Ended";
            break;
        case TransactionEventData::Type::Started:
            typeCstr = "Started";
            break;
        case TransactionEventData::Type::Updated:
            typeCstr = "Updated";
            break;
    }
    return typeCstr;
}
bool deserializeTransactionEventType(const char *typeCstr, TransactionEventData::Type& typeOut) {
    if (!strcmp(typeCstr, "Ended")) {
        typeOut = TransactionEventData::Type::Ended;
    } else if (!strcmp(typeCstr, "Started")) {
        typeOut = TransactionEventData::Type::Started;
    } else if (!strcmp(typeCstr, "Updated")) {
        typeOut = TransactionEventData::Type::Updated;
    } else {
        MO_DBG_ERR("deserialization error");
        return false;
    }
    return true;
}

const char *serializeTransactionEventTriggerReason(TransactionEventTriggerReason triggerReason) {

    const char *triggerReasonCstr = nullptr;
    switch(triggerReason) {
        case TransactionEventTriggerReason::UNDEFINED:
            break;
        case TransactionEventTriggerReason::Authorized:
            triggerReasonCstr = "Authorized";
            break;
        case TransactionEventTriggerReason::CablePluggedIn:
            triggerReasonCstr = "CablePluggedIn";
            break;
        case TransactionEventTriggerReason::ChargingRateChanged:
            triggerReasonCstr = "ChargingRateChanged";
            break;
        case TransactionEventTriggerReason::ChargingStateChanged:
            triggerReasonCstr = "ChargingStateChanged";
            break;
        case TransactionEventTriggerReason::Deauthorized:
            triggerReasonCstr = "Deauthorized";
            break;
        case TransactionEventTriggerReason::EnergyLimitReached:
            triggerReasonCstr = "EnergyLimitReached";
            break;
        case TransactionEventTriggerReason::EVCommunicationLost:
            triggerReasonCstr = "EVCommunicationLost";
            break;
        case TransactionEventTriggerReason::EVConnectTimeout:
            triggerReasonCstr = "EVConnectTimeout";
            break;
        case TransactionEventTriggerReason::MeterValueClock:
            triggerReasonCstr = "MeterValueClock";
            break;
        case TransactionEventTriggerReason::MeterValuePeriodic:
            triggerReasonCstr = "MeterValuePeriodic";
            break;
        case TransactionEventTriggerReason::TimeLimitReached:
            triggerReasonCstr = "TimeLimitReached";
            break;
        case TransactionEventTriggerReason::Trigger:
            triggerReasonCstr = "Trigger";
            break;
        case TransactionEventTriggerReason::UnlockCommand:
            triggerReasonCstr = "UnlockCommand";
            break;
        case TransactionEventTriggerReason::StopAuthorized:
            triggerReasonCstr = "StopAuthorized";
            break;
        case TransactionEventTriggerReason::EVDeparted:
            triggerReasonCstr = "EVDeparted";
            break;
        case TransactionEventTriggerReason::EVDetected:
            triggerReasonCstr = "EVDetected";
            break;
        case TransactionEventTriggerReason::RemoteStop:
            triggerReasonCstr = "RemoteStop";
            break;
        case TransactionEventTriggerReason::RemoteStart:
            triggerReasonCstr = "RemoteStart";
            break;
        case TransactionEventTriggerReason::AbnormalCondition:
            triggerReasonCstr = "AbnormalCondition";
            break;
        case TransactionEventTriggerReason::SignedDataReceived:
            triggerReasonCstr = "SignedDataReceived";
            break;
        case TransactionEventTriggerReason::ResetCommand:
            triggerReasonCstr = "ResetCommand";
            break;
    }

    return triggerReasonCstr;
}
bool deserializeTransactionEventTriggerReason(const char *triggerReasonCstr, TransactionEventTriggerReason& triggerReasonOut) {
    if (!triggerReasonCstr || !*triggerReasonCstr) {
        triggerReasonOut = TransactionEventTriggerReason::UNDEFINED;
    } else if (!strcmp(triggerReasonCstr, "Authorized")) {
        triggerReasonOut = TransactionEventTriggerReason::Authorized;
    } else if (!strcmp(triggerReasonCstr, "CablePluggedIn")) {
        triggerReasonOut = TransactionEventTriggerReason::CablePluggedIn;
    } else if (!strcmp(triggerReasonCstr, "ChargingRateChanged")) {
        triggerReasonOut = TransactionEventTriggerReason::ChargingRateChanged;
    } else if (!strcmp(triggerReasonCstr, "ChargingStateChanged")) {
        triggerReasonOut = TransactionEventTriggerReason::ChargingStateChanged;
    } else if (!strcmp(triggerReasonCstr, "Deauthorized")) {
        triggerReasonOut = TransactionEventTriggerReason::Deauthorized;
    } else if (!strcmp(triggerReasonCstr, "EnergyLimitReached")) {
        triggerReasonOut = TransactionEventTriggerReason::EnergyLimitReached;
    } else if (!strcmp(triggerReasonCstr, "EVCommunicationLost")) {
        triggerReasonOut = TransactionEventTriggerReason::EVCommunicationLost;
    } else if (!strcmp(triggerReasonCstr, "EVConnectTimeout")) {
        triggerReasonOut = TransactionEventTriggerReason::EVConnectTimeout;
    } else if (!strcmp(triggerReasonCstr, "MeterValueClock")) {
        triggerReasonOut = TransactionEventTriggerReason::MeterValueClock;
    } else if (!strcmp(triggerReasonCstr, "MeterValuePeriodic")) {
        triggerReasonOut = TransactionEventTriggerReason::MeterValuePeriodic;
    } else if (!strcmp(triggerReasonCstr, "TimeLimitReached")) {
        triggerReasonOut = TransactionEventTriggerReason::TimeLimitReached;
    } else if (!strcmp(triggerReasonCstr, "Trigger")) {
        triggerReasonOut = TransactionEventTriggerReason::Trigger;
    } else if (!strcmp(triggerReasonCstr, "UnlockCommand")) {
        triggerReasonOut = TransactionEventTriggerReason::UnlockCommand;
    } else if (!strcmp(triggerReasonCstr, "StopAuthorized")) {
        triggerReasonOut = TransactionEventTriggerReason::StopAuthorized;
    } else if (!strcmp(triggerReasonCstr, "EVDeparted")) {
        triggerReasonOut = TransactionEventTriggerReason::EVDeparted;
    } else if (!strcmp(triggerReasonCstr, "EVDetected")) {
        triggerReasonOut = TransactionEventTriggerReason::EVDetected;
    } else if (!strcmp(triggerReasonCstr, "RemoteStop")) {
        triggerReasonOut = TransactionEventTriggerReason::RemoteStop;
    } else if (!strcmp(triggerReasonCstr, "RemoteStart")) {
        triggerReasonOut = TransactionEventTriggerReason::RemoteStart;
    } else if (!strcmp(triggerReasonCstr, "AbnormalCondition")) {
        triggerReasonOut = TransactionEventTriggerReason::AbnormalCondition;
    } else if (!strcmp(triggerReasonCstr, "SignedDataReceived")) {
        triggerReasonOut = TransactionEventTriggerReason::SignedDataReceived;
    } else if (!strcmp(triggerReasonCstr, "ResetCommand")) {
        triggerReasonOut = TransactionEventTriggerReason::ResetCommand;
    } else {
        MO_DBG_ERR("deserialization error");
        return false;
    }
    return true;
}

const char *serializeTransactionEventChargingState(TransactionEventData::ChargingState chargingState) {
    const char *chargingStateCstr = nullptr;
    switch (chargingState) {
        case TransactionEventData::ChargingState::UNDEFINED:
            // optional, okay
            break;
        case TransactionEventData::ChargingState::Charging:
            chargingStateCstr = "Charging";
            break;
        case TransactionEventData::ChargingState::EVConnected:
            chargingStateCstr = "EVConnected";
            break;
        case TransactionEventData::ChargingState::SuspendedEV:
            chargingStateCstr = "SuspendedEV";
            break;
        case TransactionEventData::ChargingState::SuspendedEVSE:
            chargingStateCstr = "SuspendedEVSE";
            break;
        case TransactionEventData::ChargingState::Idle:
            chargingStateCstr = "Idle";
            break;
    }
    return chargingStateCstr;
}
bool deserializeTransactionEventChargingState(const char *chargingStateCstr, TransactionEventData::ChargingState& chargingStateOut) {
    if (!chargingStateCstr || !*chargingStateCstr) {
        chargingStateOut = TransactionEventData::ChargingState::UNDEFINED;
    } else if (!strcmp(chargingStateCstr, "Charging")) {
        chargingStateOut = TransactionEventData::ChargingState::Charging;
    } else if (!strcmp(chargingStateCstr, "EVConnected")) {
        chargingStateOut = TransactionEventData::ChargingState::EVConnected;
    } else if (!strcmp(chargingStateCstr, "SuspendedEV")) {
        chargingStateOut = TransactionEventData::ChargingState::SuspendedEV;
    } else if (!strcmp(chargingStateCstr, "SuspendedEVSE")) {
        chargingStateOut = TransactionEventData::ChargingState::SuspendedEVSE;
    } else if (!strcmp(chargingStateCstr, "Idle")) {
        chargingStateOut = TransactionEventData::ChargingState::Idle;
    } else {
        MO_DBG_ERR("deserialization error");
        return false;
    }
    return true;
}

} //namespace Ocpp201
} //namespace MicroOcpp

#endif //MO_ENABLE_V201

#if MO_ENABLE_V201
bool g_ocpp_tx_compat_v201;

void ocpp_tx_compat_setV201(bool isV201) {
    g_ocpp_tx_compat_v201 = isV201;
}
#endif

int ocpp_tx_getTransactionId(OCPP_Transaction *tx) {
    #if MO_ENABLE_V201
    if (g_ocpp_tx_compat_v201) {
        MO_DBG_ERR("only supported in v16");
        return -1;
    }
    #endif //MO_ENABLE_V201
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->getTransactionId();
}
#if MO_ENABLE_V201
const char *ocpp_tx_getTransactionIdV201(OCPP_Transaction *tx) {
    if (!g_ocpp_tx_compat_v201) {
        MO_DBG_ERR("only supported in v201");
        return nullptr;
    }
    return reinterpret_cast<MicroOcpp::Ocpp201::Transaction*>(tx)->transactionId;
}
#endif //MO_ENABLE_V201
bool ocpp_tx_isAuthorized(OCPP_Transaction *tx) {
    #if MO_ENABLE_V201
    if (g_ocpp_tx_compat_v201) {
        return reinterpret_cast<MicroOcpp::Ocpp201::Transaction*>(tx)->isAuthorized;
    }
    #endif //MO_ENABLE_V201
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->isAuthorized();
}
bool ocpp_tx_isIdTagDeauthorized(OCPP_Transaction *tx) {
    #if MO_ENABLE_V201
    if (g_ocpp_tx_compat_v201) {
        return reinterpret_cast<MicroOcpp::Ocpp201::Transaction*>(tx)->isDeauthorized;
    }
    #endif //MO_ENABLE_V201
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->isIdTagDeauthorized();
}

bool ocpp_tx_isRunning(OCPP_Transaction *tx) {
    #if MO_ENABLE_V201
    if (g_ocpp_tx_compat_v201) {
        auto transaction = reinterpret_cast<MicroOcpp::Ocpp201::Transaction*>(tx);
        return transaction->started && !transaction->stopped;
    }
    #endif //MO_ENABLE_V201
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->isRunning();
}
bool ocpp_tx_isActive(OCPP_Transaction *tx) {
    #if MO_ENABLE_V201
    if (g_ocpp_tx_compat_v201) {
        return reinterpret_cast<MicroOcpp::Ocpp201::Transaction*>(tx)->active;
    }
    #endif //MO_ENABLE_V201
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->isActive();
}
bool ocpp_tx_isAborted(OCPP_Transaction *tx) {
    #if MO_ENABLE_V201
    if (g_ocpp_tx_compat_v201) {
        auto transaction = reinterpret_cast<MicroOcpp::Ocpp201::Transaction*>(tx);
        return !transaction->active && !transaction->started;
    }
    #endif //MO_ENABLE_V201
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->isAborted();
}
bool ocpp_tx_isCompleted(OCPP_Transaction *tx) {
    #if MO_ENABLE_V201
    if (g_ocpp_tx_compat_v201) {
        auto transaction = reinterpret_cast<MicroOcpp::Ocpp201::Transaction*>(tx);
        return transaction->stopped && transaction->seqNos.empty();
    }
    #endif //MO_ENABLE_V201
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->isCompleted();
}

const char *ocpp_tx_getIdTag(OCPP_Transaction *tx) {
    #if MO_ENABLE_V201
    if (g_ocpp_tx_compat_v201) {
        auto transaction = reinterpret_cast<MicroOcpp::Ocpp201::Transaction*>(tx);
        return transaction->idToken.get();
    }
    #endif //MO_ENABLE_V201
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->getIdTag();
}

const char *ocpp_tx_getParentIdTag(OCPP_Transaction *tx) {
    #if MO_ENABLE_V201
    if (g_ocpp_tx_compat_v201) {
        MO_DBG_ERR("only supported in v16");
        return nullptr;
    }
    #endif //MO_ENABLE_V201
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->getParentIdTag();
}

bool ocpp_tx_getBeginTimestamp(OCPP_Transaction *tx, char *buf, size_t len) {
    #if MO_ENABLE_V201
    if (g_ocpp_tx_compat_v201) {
        return reinterpret_cast<MicroOcpp::Ocpp201::Transaction*>(tx)->beginTimestamp.toJsonString(buf, len);
    }
    #endif //MO_ENABLE_V201
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->getBeginTimestamp().toJsonString(buf, len);
}

int32_t ocpp_tx_getMeterStart(OCPP_Transaction *tx) {
    #if MO_ENABLE_V201
    if (g_ocpp_tx_compat_v201) {
        MO_DBG_ERR("only supported in v16");
        return -1;
    }
    #endif //MO_ENABLE_V201
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->getMeterStart();
}

bool ocpp_tx_getStartTimestamp(OCPP_Transaction *tx, char *buf, size_t len) {
    #if MO_ENABLE_V201
    if (g_ocpp_tx_compat_v201) {
        MO_DBG_ERR("only supported in v16");
        return -1;
    }
    #endif //MO_ENABLE_V201
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->getStartTimestamp().toJsonString(buf, len);
}

const char *ocpp_tx_getStopIdTag(OCPP_Transaction *tx) {
    #if MO_ENABLE_V201
    if (g_ocpp_tx_compat_v201) {
        auto transaction = reinterpret_cast<MicroOcpp::Ocpp201::Transaction*>(tx);
        return transaction->stopIdToken ? transaction->stopIdToken->get() : "";
    }
    #endif //MO_ENABLE_V201
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->getStopIdTag();
}

int32_t ocpp_tx_getMeterStop(OCPP_Transaction *tx) {
    #if MO_ENABLE_V201
    if (g_ocpp_tx_compat_v201) {
        MO_DBG_ERR("only supported in v16");
        return -1;
    }
    #endif //MO_ENABLE_V201
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->getMeterStop();
}

void ocpp_tx_setMeterStop(OCPP_Transaction* tx, int32_t meter) {
    #if MO_ENABLE_V201
    if (g_ocpp_tx_compat_v201) {
        MO_DBG_ERR("only supported in v16");
        return;
    }
    #endif //MO_ENABLE_V201
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->setMeterStop(meter);
}

bool ocpp_tx_getStopTimestamp(OCPP_Transaction *tx, char *buf, size_t len) {
    #if MO_ENABLE_V201
    if (g_ocpp_tx_compat_v201) {
        MO_DBG_ERR("only supported in v16");
        return -1;
    }
    #endif //MO_ENABLE_V201
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->getStopTimestamp().toJsonString(buf, len);
}

const char *ocpp_tx_getStopReason(OCPP_Transaction *tx) {
    #if MO_ENABLE_V201
    if (g_ocpp_tx_compat_v201) {
        auto transaction = reinterpret_cast<MicroOcpp::Ocpp201::Transaction*>(tx);
        return serializeTransactionStoppedReason(transaction->stoppedReason);
    }
    #endif //MO_ENABLE_V201
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->getStopReason();
}
