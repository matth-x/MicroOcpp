// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16

using namespace MicroOcpp;

v16::Transaction::Transaction(unsigned int connectorId, unsigned int txNr, bool silent) : 
                MemoryManaged("v16.Transactions.Transaction"),
                connectorId(connectorId), 
                txNr(txNr),
                silent(silent),
                meterValues(makeVector<MeterValue*>("v16.Transactions.TransactionMeterData")) { }

v16::Transaction::~Transaction() {
    for (size_t i = 0; i < meterValues.size(); i++) {
        delete meterValues[i];
    }
    meterValues.clear();
}

void v16::Transaction::setTransactionId(int transactionId) {
    this->transactionId = transactionId;
    #if MO_ENABLE_V201
    if (transactionId > 0) {
        snprintf(transactionIdCompat, sizeof(transactionIdCompat), "%d", transactionId);
    } else {
        transactionIdCompat[0] = '\0';
    }
    #endif //MO_ENABLE_V201
}

bool v16::Transaction::setIdTag(const char *idTag) {
    auto ret = snprintf(this->idTag, MO_IDTAG_LEN_MAX + 1, "%s", idTag);
    return ret >= 0 && ret < MO_IDTAG_LEN_MAX + 1;
}

bool v16::Transaction::setParentIdTag(const char *idTag) {
    auto ret = snprintf(this->parentIdTag, MO_IDTAG_LEN_MAX + 1, "%s", idTag);
    return ret >= 0 && ret < MO_IDTAG_LEN_MAX + 1;
}

bool v16::Transaction::setStopIdTag(const char *idTag) {
    auto ret = snprintf(stop_idTag, MO_IDTAG_LEN_MAX + 1, "%s", idTag);
    return ret >= 0 && ret < MO_IDTAG_LEN_MAX + 1;
}

bool v16::Transaction::setStopReason(const char *reason) {
    auto ret = snprintf(stop_reason, sizeof(stop_reason) + 1, "%s", reason);
    return ret >= 0 && (size_t)ret < sizeof(stop_reason) + 1;
}

#endif //MO_ENABLE_V16

#if MO_ENABLE_V201

namespace MicroOcpp {
namespace v201 {

const char *serializeTransactionStoppedReason(MO_TxStoppedReason stoppedReason) {
    const char *stoppedReasonCstr = nullptr;
    switch (stoppedReason) {
        case MO_TxStoppedReason_UNDEFINED:
            // optional, okay
            break;
        case MO_TxStoppedReason_Local:
            stoppedReasonCstr = "Local";
            break;
        case MO_TxStoppedReason_DeAuthorized:
            stoppedReasonCstr = "DeAuthorized";
            break;
        case MO_TxStoppedReason_EmergencyStop:
            stoppedReasonCstr = "EmergencyStop";
            break;
        case MO_TxStoppedReason_EnergyLimitReached:
            stoppedReasonCstr = "EnergyLimitReached";
            break;
        case MO_TxStoppedReason_EVDisconnected:
            stoppedReasonCstr = "EVDisconnected";
            break;
        case MO_TxStoppedReason_GroundFault:
            stoppedReasonCstr = "GroundFault";
            break;
        case MO_TxStoppedReason_ImmediateReset:
            stoppedReasonCstr = "ImmediateReset";
            break;
        case MO_TxStoppedReason_LocalOutOfCredit:
            stoppedReasonCstr = "LocalOutOfCredit";
            break;
        case MO_TxStoppedReason_MasterPass:
            stoppedReasonCstr = "MasterPass";
            break;
        case MO_TxStoppedReason_Other:
            stoppedReasonCstr = "Other";
            break;
        case MO_TxStoppedReason_OvercurrentFault:
            stoppedReasonCstr = "OvercurrentFault";
            break;
        case MO_TxStoppedReason_PowerLoss:
            stoppedReasonCstr = "PowerLoss";
            break;
        case MO_TxStoppedReason_PowerQuality:
            stoppedReasonCstr = "PowerQuality";
            break;
        case MO_TxStoppedReason_Reboot:
            stoppedReasonCstr = "Reboot";
            break;
        case MO_TxStoppedReason_Remote:
            stoppedReasonCstr = "Remote";
            break;
        case MO_TxStoppedReason_SOCLimitReached:
            stoppedReasonCstr = "SOCLimitReached";
            break;
        case MO_TxStoppedReason_StoppedByEV:
            stoppedReasonCstr = "StoppedByEV";
            break;
        case MO_TxStoppedReason_TimeLimitReached:
            stoppedReasonCstr = "TimeLimitReached";
            break;
        case MO_TxStoppedReason_Timeout:
            stoppedReasonCstr = "Timeout";
            break;
    }

    return stoppedReasonCstr;
}
bool deserializeTransactionStoppedReason(const char *stoppedReasonCstr, MO_TxStoppedReason& stoppedReasonOut) {
    if (!stoppedReasonCstr || !*stoppedReasonCstr) {
        stoppedReasonOut = MO_TxStoppedReason_UNDEFINED;
    } else if (!strcmp(stoppedReasonCstr, "DeAuthorized")) {
        stoppedReasonOut = MO_TxStoppedReason_DeAuthorized;
    } else if (!strcmp(stoppedReasonCstr, "EmergencyStop")) {
        stoppedReasonOut = MO_TxStoppedReason_EmergencyStop;
    } else if (!strcmp(stoppedReasonCstr, "EnergyLimitReached")) {
        stoppedReasonOut = MO_TxStoppedReason_EnergyLimitReached;
    } else if (!strcmp(stoppedReasonCstr, "EVDisconnected")) {
        stoppedReasonOut = MO_TxStoppedReason_EVDisconnected;
    } else if (!strcmp(stoppedReasonCstr, "GroundFault")) {
        stoppedReasonOut = MO_TxStoppedReason_GroundFault;
    } else if (!strcmp(stoppedReasonCstr, "ImmediateReset")) {
        stoppedReasonOut = MO_TxStoppedReason_ImmediateReset;
    } else if (!strcmp(stoppedReasonCstr, "Local")) {
        stoppedReasonOut = MO_TxStoppedReason_Local;
    } else if (!strcmp(stoppedReasonCstr, "LocalOutOfCredit")) {
        stoppedReasonOut = MO_TxStoppedReason_LocalOutOfCredit;
    } else if (!strcmp(stoppedReasonCstr, "MasterPass")) {
        stoppedReasonOut = MO_TxStoppedReason_MasterPass;
    } else if (!strcmp(stoppedReasonCstr, "Other")) {
        stoppedReasonOut = MO_TxStoppedReason_Other;
    } else if (!strcmp(stoppedReasonCstr, "OvercurrentFault")) {
        stoppedReasonOut = MO_TxStoppedReason_OvercurrentFault;
    } else if (!strcmp(stoppedReasonCstr, "PowerLoss")) {
        stoppedReasonOut = MO_TxStoppedReason_PowerLoss;
    } else if (!strcmp(stoppedReasonCstr, "PowerQuality")) {
        stoppedReasonOut = MO_TxStoppedReason_PowerQuality;
    } else if (!strcmp(stoppedReasonCstr, "Reboot")) {
        stoppedReasonOut = MO_TxStoppedReason_Reboot;
    } else if (!strcmp(stoppedReasonCstr, "Remote")) {
        stoppedReasonOut = MO_TxStoppedReason_Remote;
    } else if (!strcmp(stoppedReasonCstr, "SOCLimitReached")) {
        stoppedReasonOut = MO_TxStoppedReason_SOCLimitReached;
    } else if (!strcmp(stoppedReasonCstr, "StoppedByEV")) {
        stoppedReasonOut = MO_TxStoppedReason_StoppedByEV;
    } else if (!strcmp(stoppedReasonCstr, "TimeLimitReached")) {
        stoppedReasonOut = MO_TxStoppedReason_TimeLimitReached;
    } else if (!strcmp(stoppedReasonCstr, "Timeout")) {
        stoppedReasonOut = MO_TxStoppedReason_Timeout;
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

const char *serializeTxEventTriggerReason(MO_TxEventTriggerReason triggerReason) {

    const char *triggerReasonCstr = nullptr;
    switch(triggerReason) {
        case MO_TxEventTriggerReason_UNDEFINED:
            break;
        case MO_TxEventTriggerReason_Authorized:
            triggerReasonCstr = "Authorized";
            break;
        case MO_TxEventTriggerReason_CablePluggedIn:
            triggerReasonCstr = "CablePluggedIn";
            break;
        case MO_TxEventTriggerReason_ChargingRateChanged:
            triggerReasonCstr = "ChargingRateChanged";
            break;
        case MO_TxEventTriggerReason_ChargingStateChanged:
            triggerReasonCstr = "ChargingStateChanged";
            break;
        case MO_TxEventTriggerReason_Deauthorized:
            triggerReasonCstr = "Deauthorized";
            break;
        case MO_TxEventTriggerReason_EnergyLimitReached:
            triggerReasonCstr = "EnergyLimitReached";
            break;
        case MO_TxEventTriggerReason_EVCommunicationLost:
            triggerReasonCstr = "EVCommunicationLost";
            break;
        case MO_TxEventTriggerReason_EVConnectTimeout:
            triggerReasonCstr = "EVConnectTimeout";
            break;
        case MO_TxEventTriggerReason_MeterValueClock:
            triggerReasonCstr = "MeterValueClock";
            break;
        case MO_TxEventTriggerReason_MeterValuePeriodic:
            triggerReasonCstr = "MeterValuePeriodic";
            break;
        case MO_TxEventTriggerReason_TimeLimitReached:
            triggerReasonCstr = "TimeLimitReached";
            break;
        case MO_TxEventTriggerReason_Trigger:
            triggerReasonCstr = "Trigger";
            break;
        case MO_TxEventTriggerReason_UnlockCommand:
            triggerReasonCstr = "UnlockCommand";
            break;
        case MO_TxEventTriggerReason_StopAuthorized:
            triggerReasonCstr = "StopAuthorized";
            break;
        case MO_TxEventTriggerReason_EVDeparted:
            triggerReasonCstr = "EVDeparted";
            break;
        case MO_TxEventTriggerReason_EVDetected:
            triggerReasonCstr = "EVDetected";
            break;
        case MO_TxEventTriggerReason_RemoteStop:
            triggerReasonCstr = "RemoteStop";
            break;
        case MO_TxEventTriggerReason_RemoteStart:
            triggerReasonCstr = "RemoteStart";
            break;
        case MO_TxEventTriggerReason_AbnormalCondition:
            triggerReasonCstr = "AbnormalCondition";
            break;
        case MO_TxEventTriggerReason_SignedDataReceived:
            triggerReasonCstr = "SignedDataReceived";
            break;
        case MO_TxEventTriggerReason_ResetCommand:
            triggerReasonCstr = "ResetCommand";
            break;
    }

    return triggerReasonCstr;
}
bool deserializeTxEventTriggerReason(const char *triggerReasonCstr, MO_TxEventTriggerReason& triggerReasonOut) {
    if (!triggerReasonCstr || !*triggerReasonCstr) {
        triggerReasonOut = MO_TxEventTriggerReason_UNDEFINED;
    } else if (!strcmp(triggerReasonCstr, "Authorized")) {
        triggerReasonOut = MO_TxEventTriggerReason_Authorized;
    } else if (!strcmp(triggerReasonCstr, "CablePluggedIn")) {
        triggerReasonOut = MO_TxEventTriggerReason_CablePluggedIn;
    } else if (!strcmp(triggerReasonCstr, "ChargingRateChanged")) {
        triggerReasonOut = MO_TxEventTriggerReason_ChargingRateChanged;
    } else if (!strcmp(triggerReasonCstr, "ChargingStateChanged")) {
        triggerReasonOut = MO_TxEventTriggerReason_ChargingStateChanged;
    } else if (!strcmp(triggerReasonCstr, "Deauthorized")) {
        triggerReasonOut = MO_TxEventTriggerReason_Deauthorized;
    } else if (!strcmp(triggerReasonCstr, "EnergyLimitReached")) {
        triggerReasonOut = MO_TxEventTriggerReason_EnergyLimitReached;
    } else if (!strcmp(triggerReasonCstr, "EVCommunicationLost")) {
        triggerReasonOut = MO_TxEventTriggerReason_EVCommunicationLost;
    } else if (!strcmp(triggerReasonCstr, "EVConnectTimeout")) {
        triggerReasonOut = MO_TxEventTriggerReason_EVConnectTimeout;
    } else if (!strcmp(triggerReasonCstr, "MeterValueClock")) {
        triggerReasonOut = MO_TxEventTriggerReason_MeterValueClock;
    } else if (!strcmp(triggerReasonCstr, "MeterValuePeriodic")) {
        triggerReasonOut = MO_TxEventTriggerReason_MeterValuePeriodic;
    } else if (!strcmp(triggerReasonCstr, "TimeLimitReached")) {
        triggerReasonOut = MO_TxEventTriggerReason_TimeLimitReached;
    } else if (!strcmp(triggerReasonCstr, "Trigger")) {
        triggerReasonOut = MO_TxEventTriggerReason_Trigger;
    } else if (!strcmp(triggerReasonCstr, "UnlockCommand")) {
        triggerReasonOut = MO_TxEventTriggerReason_UnlockCommand;
    } else if (!strcmp(triggerReasonCstr, "StopAuthorized")) {
        triggerReasonOut = MO_TxEventTriggerReason_StopAuthorized;
    } else if (!strcmp(triggerReasonCstr, "EVDeparted")) {
        triggerReasonOut = MO_TxEventTriggerReason_EVDeparted;
    } else if (!strcmp(triggerReasonCstr, "EVDetected")) {
        triggerReasonOut = MO_TxEventTriggerReason_EVDetected;
    } else if (!strcmp(triggerReasonCstr, "RemoteStop")) {
        triggerReasonOut = MO_TxEventTriggerReason_RemoteStop;
    } else if (!strcmp(triggerReasonCstr, "RemoteStart")) {
        triggerReasonOut = MO_TxEventTriggerReason_RemoteStart;
    } else if (!strcmp(triggerReasonCstr, "AbnormalCondition")) {
        triggerReasonOut = MO_TxEventTriggerReason_AbnormalCondition;
    } else if (!strcmp(triggerReasonCstr, "SignedDataReceived")) {
        triggerReasonOut = MO_TxEventTriggerReason_SignedDataReceived;
    } else if (!strcmp(triggerReasonCstr, "ResetCommand")) {
        triggerReasonOut = MO_TxEventTriggerReason_ResetCommand;
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

} //namespace v201
} //namespace MicroOcpp

#endif //MO_ENABLE_V201
