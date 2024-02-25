// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Operations/TransactionEvent.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp201::TransactionEvent;

TransactionEvent::TransactionEvent(Model& model, std::shared_ptr<TransactionEventData> txEvent)
        : model(model), txEvent(txEvent) {

}

const char* TransactionEvent::getOperationType() {
    return "TransactionEvent";
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
        case TransactionEventData::TriggerReason::Authorized:
            triggerReason = "Authorized";
            break;
        case TransactionEventData::TriggerReason::CablePluggedIn:
            triggerReason = "CablePluggedIn";
            break;
        case TransactionEventData::TriggerReason::ChargingRateChanged:
            triggerReason = "ChargingRateChanged";
            break;
        case TransactionEventData::TriggerReason::ChargingStateChanged:
            triggerReason = "ChargingStateChanged";
            break;
        case TransactionEventData::TriggerReason::Deauthorized:
            triggerReason = "Deauthorized";
            break;
        case TransactionEventData::TriggerReason::EnergyLimitReached:
            triggerReason = "EnergyLimitReached";
            break;
        case TransactionEventData::TriggerReason::EVCommunicationLost:
            triggerReason = "EVCommunicationLost";
            break;
        case TransactionEventData::TriggerReason::EVConnectTimeout:
            triggerReason = "EVConnectTimeout";
            break;
        case TransactionEventData::TriggerReason::MeterValueClock:
            triggerReason = "MeterValueClock";
            break;
        case TransactionEventData::TriggerReason::MeterValuePeriodic:
            triggerReason = "MeterValuePeriodic";
            break;
        case TransactionEventData::TriggerReason::TimeLimitReached:
            triggerReason = "TimeLimitReached";
            break;
        case TransactionEventData::TriggerReason::Trigger:
            triggerReason = "Trigger";
            break;
        case TransactionEventData::TriggerReason::UnlockCommand:
            triggerReason = "UnlockCommand";
            break;
        case TransactionEventData::TriggerReason::StopAuthorized:
            triggerReason = "StopAuthorized";
            break;
        case TransactionEventData::TriggerReason::EVDeparted:
            triggerReason = "EVDeparted";
            break;
        case TransactionEventData::TriggerReason::EVDetected:
            triggerReason = "EVDetected";
            break;
        case TransactionEventData::TriggerReason::RemoteStop:
            triggerReason = "RemoteStop";
            break;
        case TransactionEventData::TriggerReason::RemoteStart:
            triggerReason = "RemoteStart";
            break;
        case TransactionEventData::TriggerReason::AbnormalCondition:
            triggerReason = "AbnormalCondition";
            break;
        case TransactionEventData::TriggerReason::SignedDataReceived:
            triggerReason = "SignedDataReceived";
            break;
        case TransactionEventData::TriggerReason::ResetCommand:
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
    transactionInfo["transactionId"] = txEvent->transaction.transactionId;

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
    switch (txEvent->stoppedReason) {
        case TransactionEventData::StopReason::DeAuthorized: 
            stoppedReason = "DeAuthorized"; 
            break;
        case TransactionEventData::StopReason::EmergencyStop: 
            stoppedReason = "EmergencyStop"; 
            break;
        case TransactionEventData::StopReason::EnergyLimitReached: 
            stoppedReason = "EnergyLimitReached";
            break;
        case TransactionEventData::StopReason::EVDisconnected: 
            stoppedReason = "EVDisconnected";
            break;
        case TransactionEventData::StopReason::GroundFault: 
            stoppedReason = "GroundFault";
            break;
        case TransactionEventData::StopReason::ImmediateReset:
            stoppedReason = "ImmediateReset";
            break;
        case TransactionEventData::StopReason::LocalOutOfCredit:
            stoppedReason = "LocalOutOfCredit";
            break;
        case TransactionEventData::StopReason::MasterPass:
            stoppedReason = "MasterPass";
            break;
        case TransactionEventData::StopReason::Other:
            stoppedReason = "Other";
            break;
        case TransactionEventData::StopReason::OvercurrentFault:
            stoppedReason = "OvercurrentFault";
            break;
        case TransactionEventData::StopReason::PowerLoss:
            stoppedReason = "PowerLoss";
            break;
        case TransactionEventData::StopReason::PowerQuality:
            stoppedReason = "PowerQuality";
            break;
        case TransactionEventData::StopReason::Reboot:
            stoppedReason = "Reboot";
            break;
        case TransactionEventData::StopReason::Remote:
            stoppedReason = "Remote";
            break;
        case TransactionEventData::StopReason::SOCLimitReached:
            stoppedReason = "SOCLimitReached";
            break;
        case TransactionEventData::StopReason::StoppedByEV:
            stoppedReason = "StoppedByEV";
            break;
        case TransactionEventData::StopReason::TimeLimitReached:
            stoppedReason = "TimeLimitReached";
            break;
        case TransactionEventData::StopReason::Timeout:
            stoppedReason = "Timeout";
            break;
    }
    if (stoppedReason) { // optional
        transactionInfo["stoppedReason"] = stoppedReason;
    }

    if (txEvent->transaction.remoteStartId >= 0) {
        payload["remoteStartId"] = txEvent->transaction.remoteStartId;
    }

    if (txEvent->idTokenTransmit) {
        JsonObject idToken = payload.createNestedObject("idToken");
        idToken["idToken"] = txEvent->transaction.idToken.get();
        idToken["type"] = txEvent->transaction.idToken.getTypeCstr();
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
            txEvent->transaction.active = false;
            txEvent->transaction.isDeauthorized = true;
        }
    }
}
