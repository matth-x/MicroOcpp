// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Operations/TransactionEvent.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp201::TransactionEvent;
using namespace MicroOcpp::Ocpp201;

TransactionEvent::TransactionEvent(Model& model, std::shared_ptr<TransactionEventData> txEvent)
        : model(model), txEvent(txEvent) {

}

const char* TransactionEvent::getOperationType() {
    return "TransactionEvent";
}

void TransactionEvent::initiate(StoredOperationHandler *opStore) {
    if (!txEvent || !txEvent->transaction || !txEvent->transaction) {
        MO_DBG_ERR("initialization error");
        return;
    }

    auto transaction = txEvent->transaction;

    if (txEvent->eventType == TransactionEventData::Type::Started) {
        if (transaction->started) {
            MO_DBG_ERR("initialization error");
            return;
        }

        transaction->started = true;
    }

    if (txEvent->eventType == TransactionEventData::Type::Ended) {
        if (transaction->stopped) {
            MO_DBG_ERR("initialization error");
            return;
        }

        transaction->stopped = true;
    }

    //commit operation and tx

    MO_DBG_INFO("TransactionEvent initiated");
}

std::unique_ptr<DynamicJsonDocument> TransactionEvent::createReq() {
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
                JSON_OBJECT_SIZE(12) + //total of 12 fields
                JSONDATE_LENGTH + 1 + //timestamp string
                JSON_OBJECT_SIZE(5) + //transactionInfo
                    MO_TXID_LEN_MAX + 1 + //transactionId
                MO_IDTOKEN_LEN_MAX + 1)); //idToken
                //meterValue not supported
    JsonObject payload = doc->to<JsonObject>();

    const char *eventType = "";
    switch (txEvent->eventType) {
        case TransactionEventData::Type::Ended:
            eventType = "Ended";
            break;
        case TransactionEventData::Type::Started:
            eventType = "Started";
            break;
        case TransactionEventData::Type::Updated:
            eventType = "Updated";
            break;
    }

    payload["eventType"] = eventType;

    char timestamp [JSONDATE_LENGTH + 1];
    txEvent->timestamp.toJsonString(timestamp, JSONDATE_LENGTH + 1);
    payload["timestamp"] = timestamp;

    const char *triggerReason = "";
    switch(txEvent->triggerReason) {
        case TransactionEventTriggerReason::Authorized:
            triggerReason = "Authorized";
            break;
        case TransactionEventTriggerReason::CablePluggedIn:
            triggerReason = "CablePluggedIn";
            break;
        case TransactionEventTriggerReason::ChargingRateChanged:
            triggerReason = "ChargingRateChanged";
            break;
        case TransactionEventTriggerReason::ChargingStateChanged:
            triggerReason = "ChargingStateChanged";
            break;
        case TransactionEventTriggerReason::Deauthorized:
            triggerReason = "Deauthorized";
            break;
        case TransactionEventTriggerReason::EnergyLimitReached:
            triggerReason = "EnergyLimitReached";
            break;
        case TransactionEventTriggerReason::EVCommunicationLost:
            triggerReason = "EVCommunicationLost";
            break;
        case TransactionEventTriggerReason::EVConnectTimeout:
            triggerReason = "EVConnectTimeout";
            break;
        case TransactionEventTriggerReason::MeterValueClock:
            triggerReason = "MeterValueClock";
            break;
        case TransactionEventTriggerReason::MeterValuePeriodic:
            triggerReason = "MeterValuePeriodic";
            break;
        case TransactionEventTriggerReason::TimeLimitReached:
            triggerReason = "TimeLimitReached";
            break;
        case TransactionEventTriggerReason::Trigger:
            triggerReason = "Trigger";
            break;
        case TransactionEventTriggerReason::UnlockCommand:
            triggerReason = "UnlockCommand";
            break;
        case TransactionEventTriggerReason::StopAuthorized:
            triggerReason = "StopAuthorized";
            break;
        case TransactionEventTriggerReason::EVDeparted:
            triggerReason = "EVDeparted";
            break;
        case TransactionEventTriggerReason::EVDetected:
            triggerReason = "EVDetected";
            break;
        case TransactionEventTriggerReason::RemoteStop:
            triggerReason = "RemoteStop";
            break;
        case TransactionEventTriggerReason::RemoteStart:
            triggerReason = "RemoteStart";
            break;
        case TransactionEventTriggerReason::AbnormalCondition:
            triggerReason = "AbnormalCondition";
            break;
        case TransactionEventTriggerReason::SignedDataReceived:
            triggerReason = "SignedDataReceived";
            break;
        case TransactionEventTriggerReason::ResetCommand:
            triggerReason = "ResetCommand";
            break;
    }

    payload["triggerReason"] = triggerReason;

    payload["seqNo"] = txEvent->seqNo;

    if (txEvent->offline) {
        payload["offline"] = txEvent->offline;
    }

    if (txEvent->numberOfPhasesUsed >= 0) {
        payload["numberOfPhasesUsed"] = txEvent->numberOfPhasesUsed;
    }

    if (txEvent->cableMaxCurrent >= 0) {
        payload["cableMaxCurrent"] = txEvent->cableMaxCurrent;
    }

    if (txEvent->reservationId >= 0) {
        payload["reservationId"] = txEvent->reservationId;
    }

    JsonObject transactionInfo = payload.createNestedObject("transactionInfo");
    transactionInfo["transactionId"] = txEvent->transaction->transactionId;

    const char *chargingState = nullptr;
    switch (txEvent->chargingState) {
        case TransactionEventData::ChargingState::Charging:
            chargingState = "Charging";
            break;
        case TransactionEventData::ChargingState::EVConnected:
            chargingState = "EVConnected";
            break;
        case TransactionEventData::ChargingState::SuspendedEV:
            chargingState = "SuspendedEV";
            break;
        case TransactionEventData::ChargingState::SuspendedEVSE:
            chargingState = "SuspendedEVSE";
            break;
        case TransactionEventData::ChargingState::Idle:
            chargingState = "Idle";
            break;
    }
    if (chargingState) { // optional
        transactionInfo["chargingState"] = chargingState;
    }

    const char *stoppedReason = nullptr;
    switch (txEvent->transaction->stopReason) {
        case Transaction::StopReason::DeAuthorized: 
            stoppedReason = "DeAuthorized"; 
            break;
        case Transaction::StopReason::EmergencyStop: 
            stoppedReason = "EmergencyStop"; 
            break;
        case Transaction::StopReason::EnergyLimitReached: 
            stoppedReason = "EnergyLimitReached";
            break;
        case Transaction::StopReason::EVDisconnected: 
            stoppedReason = "EVDisconnected";
            break;
        case Transaction::StopReason::GroundFault: 
            stoppedReason = "GroundFault";
            break;
        case Transaction::StopReason::ImmediateReset:
            stoppedReason = "ImmediateReset";
            break;
        case Transaction::StopReason::LocalOutOfCredit:
            stoppedReason = "LocalOutOfCredit";
            break;
        case Transaction::StopReason::MasterPass:
            stoppedReason = "MasterPass";
            break;
        case Transaction::StopReason::Other:
            stoppedReason = "Other";
            break;
        case Transaction::StopReason::OvercurrentFault:
            stoppedReason = "OvercurrentFault";
            break;
        case Transaction::StopReason::PowerLoss:
            stoppedReason = "PowerLoss";
            break;
        case Transaction::StopReason::PowerQuality:
            stoppedReason = "PowerQuality";
            break;
        case Transaction::StopReason::Reboot:
            stoppedReason = "Reboot";
            break;
        case Transaction::StopReason::Remote:
            stoppedReason = "Remote";
            break;
        case Transaction::StopReason::SOCLimitReached:
            stoppedReason = "SOCLimitReached";
            break;
        case Transaction::StopReason::StoppedByEV:
            stoppedReason = "StoppedByEV";
            break;
        case Transaction::StopReason::TimeLimitReached:
            stoppedReason = "TimeLimitReached";
            break;
        case Transaction::StopReason::Timeout:
            stoppedReason = "Timeout";
            break;
    }
    if (stoppedReason) { // optional
        transactionInfo["stoppedReason"] = stoppedReason;
    }

    if (txEvent->remoteStartId >= 0) {
        transactionInfo["remoteStartId"] = txEvent->transaction->remoteStartId;
    }

    if (txEvent->idToken) {
        JsonObject idToken = payload.createNestedObject("idToken");
        idToken["idToken"] = txEvent->idToken->get();
        idToken["type"] = txEvent->idToken->getTypeCstr();
    }

    if (txEvent->evse.id >= 0) {
        JsonObject evse = payload.createNestedObject("evse");
        evse["id"] = txEvent->evse.id;
        if (txEvent->evse.connectorId >= 0) {
            evse["connectorId"] = txEvent->evse.connectorId;
        }
    }

    // meterValue not supported

    return doc;
}

void TransactionEvent::processConf(JsonObject payload) {

    if (payload.containsKey("idTokenInfo")) {
        if (strcmp(payload["idTokenInfo"]["status"], "Accepted")) {
            MO_DBG_INFO("transaction deAuthorized");
            txEvent->transaction->active = false;
            txEvent->transaction->isDeauthorized = true;
        }
    }
}

void TransactionEvent::processReq(JsonObject payload) {
    /**
     * Ignore Contents of this Req-message, because this is for debug purposes only
     */
}

std::unique_ptr<DynamicJsonDocument> TransactionEvent::createConf() {
    return createEmptyDocument();
}

#endif // MO_ENABLE_V201
