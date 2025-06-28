// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Debug.h>

#include <algorithm>

#if MO_ENABLE_V16

namespace MicroOcpp {
namespace Ocpp16 {
namespace TransactionStore {

bool serializeSendStatus(Clock& clock, SendStatus& status, JsonObject out);
bool deserializeSendStatus(Clock& clock, SendStatus& status, JsonObject in);

bool serializeTransaction(Clock& clock, Transaction& tx, JsonDoc& out);
bool deserializeTransaction(Clock& clock, Transaction& tx, JsonObject in);

} //namespace TransactionStore
} //namespace Ocpp16
} //namespace MicroOcpp

using namespace MicroOcpp;

bool Ocpp16::TransactionStore::printTxFname(char *fname, size_t size, unsigned int evseId, unsigned int txNr) {
    auto ret = snprintf(fname, size, "tx-%.*u-%.*u.json",
            MO_NUM_EVSEID_DIGITS, evseId, MO_TXNR_DIGITS, txNr);
    if (ret < 0 || (size_t)ret >= size) {
        MO_DBG_ERR("fn error: %i", ret);
        return false;
    }
    return true;
}

FilesystemUtils::LoadStatus Ocpp16::TransactionStore::load(MO_FilesystemAdapter *filesystem, Context& context, unsigned int evseId, unsigned int txNr, Transaction& transaction) {

    char fname [MO_MAX_PATH_SIZE];
    if (!printTxFname(fname, sizeof(fname), evseId, txNr)) {
        MO_DBG_ERR("fname error %u %u", evseId, txNr);
        return FilesystemUtils::LoadStatus::ErrOther;
    }

    JsonDoc doc (0);
    auto ret = FilesystemUtils::loadJson(filesystem, fname, doc, "v16.Transactions.TransactionStore");
    if (ret != FilesystemUtils::LoadStatus::Success) {
        return ret;
    }

    auto& clock = context.getClock();

    if (!deserializeTransaction(clock, transaction, doc.as<JsonObject>())) {
        MO_DBG_ERR("deserialization error %s", fname);
        return FilesystemUtils::LoadStatus::ErrFileCorruption;
    }

    //success

    MO_DBG_DEBUG("Restored tx %u-%u", evseId, txNr);

    return FilesystemUtils::LoadStatus::Success;
}

FilesystemUtils::StoreStatus Ocpp16::TransactionStore::store(MO_FilesystemAdapter *filesystem, Context& context, Transaction& transaction) {
    
    char fname [MO_MAX_PATH_SIZE];
    if (!printTxFname(fname, sizeof(fname), transaction.getConnectorId(), transaction.getTxNr())) {
        MO_DBG_ERR("fname error %u %u", transaction.getConnectorId(), transaction.getTxNr());
        return FilesystemUtils::StoreStatus::ErrOther;
    }

    auto& clock = context.getClock();

    JsonDoc doc (0);
    if (!serializeTransaction(clock, transaction, doc)) {
        MO_DBG_ERR("serialization error %s", fname);
        return FilesystemUtils::StoreStatus::ErrJsonCorruption;
    }

    auto ret = FilesystemUtils::storeJson(filesystem, fname, doc);
    if (ret != FilesystemUtils::StoreStatus::Success) {
        MO_DBG_ERR("fs error %s %i", fname, (int)ret);
    }

    return ret;
}

bool Ocpp16::TransactionStore::remove(MO_FilesystemAdapter *filesystem, unsigned int evseId, unsigned int txNr) {

    char fname [MO_MAX_PATH_SIZE];
    if (!printTxFname(fname, sizeof(fname), evseId, txNr)) {
        MO_DBG_ERR("fname error %u %u", evseId, txNr);
        return false;
    }

    char path [MO_MAX_PATH_SIZE];
    if (!FilesystemUtils::printPath(filesystem, path, sizeof(path), "%s", fname)) {
        MO_DBG_ERR("fname error %u %u", evseId, txNr);
        return false;
    }

    return filesystem->remove(path);
}

bool Ocpp16::TransactionStore::serializeSendStatus(Clock& clock, SendStatus& status, JsonObject out) {
    if (status.isRequested()) {
        out["requested"] = true;
    }
    if (status.isConfirmed()) {
        out["confirmed"] = true;
    }
    out["opNr"] = status.getOpNr();
    if (status.getAttemptNr() != 0) {
        out["attemptNr"] = status.getAttemptNr();
    }

    if (status.getAttemptTime().isDefined()) {
        char attemptTime [MO_JSONDATE_SIZE];
        if (!clock.toInternalString(status.getAttemptTime(), attemptTime, sizeof(attemptTime))) {
            return false;
        }
        out["attemptTime"] = attemptTime;
    }
    return true;
}

bool Ocpp16::TransactionStore::deserializeSendStatus(Clock& clock, SendStatus& status, JsonObject in) {
    if (in["requested"] | false) {
        status.setRequested();
    }
    if (in["confirmed"] | false) {
        status.confirm();
    }
    unsigned int opNr = in["opNr"] | (unsigned int)0;
    if (opNr >= 10) { //10 is first valid tx-related opNr
        status.setOpNr(opNr);
    }
    status.setAttemptNr(in["attemptNr"] | (unsigned int)0);
    if (in.containsKey("attemptTime")) {
        Timestamp attemptTime;
        if (!clock.parseString(in["attemptTime"] | "_Invalid", attemptTime)) {
            MO_DBG_ERR("deserialization error");
            return false;
        }
        status.setAttemptTime(attemptTime);
    }
    return true;
}

bool Ocpp16::TransactionStore::serializeTransaction(Clock& clock, Transaction& tx, JsonDoc& out) {
    out = initJsonDoc("v16.Transactions.TransactionDeserialize", 1024);
    JsonObject state = out.to<JsonObject>();

    JsonObject sessionState = state.createNestedObject("session");
    if (!tx.isActive()) {
        sessionState["active"] = false;
    }
    if (tx.getIdTag()[0] != '\0') {
        sessionState["idTag"] = tx.getIdTag();
    }
    if (tx.getParentIdTag()[0] != '\0') {
        sessionState["parentIdTag"] = tx.getParentIdTag();
    }
    if (tx.isAuthorized()) {
        sessionState["authorized"] = true;
    }
    if (tx.isIdTagDeauthorized()) {
        sessionState["deauthorized"] = true;
    }
    if (tx.getBeginTimestamp().isDefined()) {
        char timeStr [MO_JSONDATE_SIZE] = {'\0'};
        if (!clock.toInternalString(tx.getBeginTimestamp(), timeStr, sizeof(timeStr))) {
            return false;
        }
        sessionState["timestamp"] = timeStr;
    }
    if (tx.getReservationId() >= 0) {
        sessionState["reservationId"] = tx.getReservationId();
    }
    if (tx.getTxProfileId() >= 0) {
        sessionState["txProfileId"] = tx.getTxProfileId();
    }

    JsonObject txStart = state.createNestedObject("start");

    if (!serializeSendStatus(clock, tx.getStartSync(), txStart)) {
        return false;
    }

    if (tx.isMeterStartDefined()) {
        txStart["meter"] = tx.getMeterStart();
    }

    if (tx.getStartTimestamp().isDefined()) {
        char startTimeStr [MO_JSONDATE_SIZE] = {'\0'};
        if (!clock.toInternalString(tx.getStartTimestamp(), startTimeStr, sizeof(startTimeStr))) {
            return false;
        }
        txStart["timestamp"] = startTimeStr;
    }

    if (tx.getStartSync().isConfirmed()) {
        txStart["transactionId"] = tx.getTransactionId();
    }

    JsonObject txStop = state.createNestedObject("stop");

    if (!serializeSendStatus(clock, tx.getStopSync(), txStop)) {
        return false;
    }

    if (tx.getStopIdTag()[0] != '\0') {
        txStop["idTag"] = tx.getStopIdTag();
    }

    if (tx.isMeterStopDefined()) {
        txStop["meter"] = tx.getMeterStop();
    }

    if (tx.getStopTimestamp().isDefined()) {
        char stopTimeStr [MO_JSONDATE_SIZE] = {'\0'};
        if (!clock.toInternalString(tx.getStopTimestamp(), stopTimeStr, sizeof(stopTimeStr))) {
            return false;
        }
        txStop["timestamp"] = stopTimeStr;
    }

    if (tx.getStopReason()[0] != '\0') {
        txStop["reason"] = tx.getStopReason();
    }

    if (tx.isSilent()) {
        state["silent"] = true;
    }

    if (out.overflowed()) {
        MO_DBG_ERR("JSON capacity exceeded");
        return false;
    }

    return true;
}

bool Ocpp16::TransactionStore::deserializeTransaction(Clock& clock, Transaction& tx, JsonObject state) {

    JsonObject sessionState = state["session"];

    if (!(sessionState["active"] | true)) {
        tx.setInactive();
    }

    if (sessionState.containsKey("idTag")) {
        if (!tx.setIdTag(sessionState["idTag"] | "")) {
            MO_DBG_ERR("read err");
            return false;
        }
    }

    if (sessionState.containsKey("parentIdTag")) {
        if (!tx.setParentIdTag(sessionState["parentIdTag"] | "")) {
            MO_DBG_ERR("read err");
            return false;
        }
    }

    if (sessionState["authorized"] | false) {
        tx.setAuthorized();
    }

    if (sessionState["deauthorized"] | false) {
        tx.setIdTagDeauthorized();
    }

    if (sessionState.containsKey("timestamp")) {
        Timestamp timestamp;
        if (!clock.parseString(sessionState["timestamp"] | "_Invalid", timestamp)) {
            MO_DBG_ERR("read err");
            return false;
        }
        tx.setBeginTimestamp(timestamp);
    }

    if (sessionState.containsKey("reservationId")) {
        tx.setReservationId(sessionState["reservationId"] | -1);
    }

    if (sessionState.containsKey("txProfileId")) {
        tx.setTxProfileId(sessionState["txProfileId"] | -1);
    }

    JsonObject txStart = state["start"];

    if (!deserializeSendStatus(clock, tx.getStartSync(), txStart)) {
        return false;
    }

    if (txStart.containsKey("meter")) {
        tx.setMeterStart(txStart["meter"] | 0);
    }

    if (txStart.containsKey("timestamp")) {
        Timestamp timestamp;
        if (!clock.parseString(txStart["timestamp"] | "_Invalid", timestamp)) {
            MO_DBG_ERR("deserialization error");
            return false;
        }
        tx.setStartTimestamp(timestamp);
    }

    if (txStart.containsKey("transactionId")) {
        tx.setTransactionId(txStart["transactionId"] | -1);
    }

    JsonObject txStop = state["stop"];

    if (!deserializeSendStatus(clock, tx.getStopSync(), txStop)) {
        return false;
    }

    if (txStop.containsKey("idTag")) {
        if (!tx.setStopIdTag(txStop["idTag"] | "")) {
            MO_DBG_ERR("read err");
            return false;
        }
    }

    if (txStop.containsKey("meter")) {
        tx.setMeterStop(txStop["meter"] | 0);
    }

    if (txStop.containsKey("timestamp")) {
        Timestamp timestamp;
        if (!clock.parseString(txStop["timestamp"] | "_Invalid", timestamp)) {
            MO_DBG_ERR("deserialization error");
            return false;
        }
        tx.setStopTimestamp(timestamp);
    }

    if (txStop.containsKey("reason")) {
        if (!tx.setStopReason(txStop["reason"] | "")) {
            MO_DBG_ERR("read err");
            return false;
        }
    }

    if (state["silent"] | false) {
        tx.setSilent();
    }

    MO_DBG_DEBUG("DUMP TX (%s)", tx.getIdTag() ? tx.getIdTag() : "idTag missing");
    MO_DBG_DEBUG("Session   | idTag %s, active: %i, authorized: %i, deauthorized: %i", tx.getIdTag(), tx.isActive(), tx.isAuthorized(), tx.isIdTagDeauthorized());
    MO_DBG_DEBUG("Start RPC | req: %i, conf: %i", tx.getStartSync().isRequested(), tx.getStartSync().isConfirmed());
    MO_DBG_DEBUG("Stop  RPC | req: %i, conf: %i",  tx.getStopSync().isRequested(), tx.getStopSync().isConfirmed());
    if (tx.isSilent()) {
        MO_DBG_DEBUG("          | silent Tx");
    }

    return true;
}

#endif //MO_ENABLE_V16

#if MO_ENABLE_V201

using namespace MicroOcpp;

bool Ocpp201::TransactionStoreEvse::serializeTransaction(Transaction& tx, JsonObject txJson) {

    if (tx.trackEvConnected) {
        txJson["trackEvConnected"] = tx.trackEvConnected;
    }

    if (tx.trackAuthorized) {
        txJson["trackAuthorized"] = tx.trackAuthorized;
    }

    if (tx.trackDataSigned) {
        txJson["trackDataSigned"] = tx.trackDataSigned;
    }

    if (tx.trackPowerPathClosed) {
        txJson["trackPowerPathClosed"] = tx.trackPowerPathClosed;
    }

    if (tx.trackEnergyTransfer) {
        txJson["trackEnergyTransfer"] = tx.trackEnergyTransfer;
    }

    if (tx.active) {
        txJson["active"] = true;
    }
    if (tx.started) {
        txJson["started"] = true;
    }
    if (tx.stopped) {
        txJson["stopped"] = true;
    }

    if (tx.isAuthorizationActive) {
        txJson["isAuthorizationActive"] = true;
    }
    if (tx.isAuthorized) {
        txJson["isAuthorized"] = true;
    }
    if (tx.isDeauthorized) {
        txJson["isDeauthorized"] = true;
    }

    if (tx.idToken.get()) {
        txJson["idToken"]["idToken"] = tx.idToken.get();
        txJson["idToken"]["type"] = tx.idToken.getTypeCstr();
    }

    if (tx.beginTimestamp.isDefined()) {
        char timeStr [MO_JSONDATE_SIZE] = {'\0'};
        if (!context.getClock().toInternalString(tx.beginTimestamp, timeStr, sizeof(timeStr))) {
            MO_DBG_ERR("serialization error");
            return false;
        }
        txJson["beginTimestamp"] = timeStr;
    }

    if (tx.startTimestamp.isDefined()) {
        char timeStr [MO_JSONDATE_SIZE] = {'\0'};
        if (!context.getClock().toInternalString(tx.startTimestamp, timeStr, sizeof(timeStr))) {
            MO_DBG_ERR("serialization error");
            return false;
        }
        txJson["startTimestamp"] = timeStr;
    }

    if (tx.remoteStartId >= 0) {
        txJson["remoteStartId"] = tx.remoteStartId;
    }

    if (tx.txProfileId >= 0) {
        txJson["txProfileId"] = tx.txProfileId;
    }

    if (tx.evConnectionTimeoutListen) {
        txJson["evConnectionTimeoutListen"] = true;
    }

    if (serializeTransactionStoppedReason(tx.stoppedReason)) { // optional
        txJson["stoppedReason"] = serializeTransactionStoppedReason(tx.stoppedReason);
    }

    if (serializeTxEventTriggerReason(tx.stopTrigger)) {
        txJson["stopTrigger"] = serializeTxEventTriggerReason(tx.stopTrigger);
    }

    if (tx.stopIdToken) {
        JsonObject stopIdToken = txJson.createNestedObject("stopIdToken");
        stopIdToken["idToken"] = tx.stopIdToken->get();
        stopIdToken["type"] = tx.stopIdToken->getTypeCstr();
    }

    //sampledDataTxEnded not supported yet

    if (tx.silent) {
        txJson["silent"] = true;
    }

    txJson["txId"] = (const char*)tx.transactionId; //force zero-copy

    return true;
}

bool Ocpp201::TransactionStoreEvse::deserializeTransaction(Transaction& tx, JsonObject txJson) {

    if (txJson.containsKey("trackEvConnected") && !txJson["trackEvConnected"].is<bool>()) {
        return false;
    }
    tx.trackEvConnected = txJson["trackEvConnected"] | false;

    if (txJson.containsKey("trackAuthorized") && !txJson["trackAuthorized"].is<bool>()) {
        return false;
    }
    tx.trackAuthorized = txJson["trackAuthorized"] | false;

    if (txJson.containsKey("trackDataSigned") && !txJson["trackDataSigned"].is<bool>()) {
        return false;
    }
    tx.trackDataSigned = txJson["trackDataSigned"] | false;

    if (txJson.containsKey("trackPowerPathClosed") && !txJson["trackPowerPathClosed"].is<bool>()) {
        return false;
    }
    tx.trackPowerPathClosed = txJson["trackPowerPathClosed"] | false;

    if (txJson.containsKey("trackEnergyTransfer") && !txJson["trackEnergyTransfer"].is<bool>()) {
        return false;
    }
    tx.trackEnergyTransfer = txJson["trackEnergyTransfer"] | false;

    if (txJson.containsKey("active") && !txJson["active"].is<bool>()) {
        return false;
    }
    tx.active = txJson["active"] | false;
    if (txJson.containsKey("started") && !txJson["started"].is<bool>()) {
        return false;
    }
    tx.started = txJson["started"] | false;

    if (txJson.containsKey("stopped") && !txJson["stopped"].is<bool>()) {
        return false;
    }
    tx.stopped = txJson["stopped"] | false;

    if (txJson.containsKey("isAuthorizationActive") && !txJson["isAuthorizationActive"].is<bool>()) {
        return false;
    }
    tx.isAuthorizationActive = txJson["isAuthorizationActive"] | false;
    if (txJson.containsKey("isAuthorized") && !txJson["isAuthorized"].is<bool>()) {
        return false;
    }
    tx.isAuthorized = txJson["isAuthorized"] | false;

    if (txJson.containsKey("isDeauthorized") && !txJson["isDeauthorized"].is<bool>()) {
        return false;
    }
    tx.isDeauthorized = txJson["isDeauthorized"] | false;

    if (txJson.containsKey("idToken")) {
        IdToken idToken;
        if (!idToken.parseCstr(
                    txJson["idToken"]["idToken"] | (const char*)nullptr, 
                    txJson["idToken"]["type"]    | (const char*)nullptr)) {
            return false;
        }
        tx.idToken = idToken;
    }

    if (txJson.containsKey("beginTimestamp")) {
        if (!context.getClock().parseString(txJson["beginTimestamp"] | "_Invalid", tx.beginTimestamp)) {
            return false;
        }
    }

    if (txJson.containsKey("startTimestamp")) {
        if (!context.getClock().parseString(txJson["startTimestamp"] | "_Invalid", tx.startTimestamp)) {
            return false;
        }
    }

    if (txJson.containsKey("remoteStartId")) {
        int remoteStartIdIn = txJson["remoteStartId"] | -1;
        if (remoteStartIdIn < 0) {
            return false;
        }
        tx.remoteStartId = remoteStartIdIn;
    }

    if (txJson.containsKey("txProfileId")) {
        int txProfileIdIn = txJson["txProfileId"] | -1;
        if (txProfileIdIn < 0) {
            return false;
        }
        tx.txProfileId = txProfileIdIn;
    }

    if (txJson.containsKey("evConnectionTimeoutListen") && !txJson["evConnectionTimeoutListen"].is<bool>()) {
        return false;
    }
    tx.evConnectionTimeoutListen = txJson["evConnectionTimeoutListen"] | false;

    MO_TxStoppedReason stoppedReason;
    if (!deserializeTransactionStoppedReason(txJson["stoppedReason"] | (const char*)nullptr, stoppedReason)) {
        return false;
    }
    tx.stoppedReason = stoppedReason;

    MO_TxEventTriggerReason stopTrigger;
    if (!deserializeTxEventTriggerReason(txJson["stopTrigger"] | (const char*)nullptr, stopTrigger)) {
        return false;
    }
    tx.stopTrigger = stopTrigger;

    if (txJson.containsKey("stopIdToken")) {
        auto stopIdToken = std::unique_ptr<IdToken>(new IdToken());
        if (!stopIdToken) {
            MO_DBG_ERR("OOM");
            return false;
        }
        if (!stopIdToken->parseCstr(
                    txJson["stopIdToken"]["idToken"] | (const char*)nullptr, 
                    txJson["stopIdToken"]["type"]    | (const char*)nullptr)) {
            return false;
        }
        tx.stopIdToken = std::move(stopIdToken);
    }

    //sampledDataTxEnded not supported yet

    if (auto txId = txJson["txId"] | (const char*)nullptr) {
        auto ret = snprintf(tx.transactionId, sizeof(tx.transactionId), "%s", txId);
        if (ret < 0 || (size_t)ret >= sizeof(tx.transactionId)) {
            return false;
        }
    } else {
        return false;
    }

    if (txJson.containsKey("silent") && !txJson["silent"].is<bool>()) {
        return false;
    }
    tx.silent = txJson["silent"] | false;

    return true;
}

bool Ocpp201::TransactionStoreEvse::serializeTransactionEvent(TransactionEventData& txEvent, JsonObject txEventJson) {
    
    if (txEvent.eventType != TransactionEventData::Type::Updated) {
        txEventJson["eventType"] = serializeTransactionEventType(txEvent.eventType);
    }

    if (txEvent.timestamp.isDefined()) {
        char timeStr [MO_JSONDATE_SIZE] = {'\0'};
        if (!context.getClock().toInternalString(txEvent.timestamp, timeStr, sizeof(timeStr))) {
            return false;
        }
        txEventJson["timestamp"] = timeStr;
    }

    if (serializeTxEventTriggerReason(txEvent.triggerReason)) {
        txEventJson["triggerReason"] = serializeTxEventTriggerReason(txEvent.triggerReason);
    }
    
    if (txEvent.offline) {
        txEventJson["offline"] = true;
    }

    if (txEvent.numberOfPhasesUsed >= 0) {
        txEventJson["numberOfPhasesUsed"] = txEvent.numberOfPhasesUsed;
    }

    if (txEvent.cableMaxCurrent >= 0) {
        txEventJson["cableMaxCurrent"] = txEvent.cableMaxCurrent;
    }

    if (txEvent.reservationId >= 0) {
        txEventJson["reservationId"] = txEvent.reservationId;
    }

    if (txEvent.remoteStartId >= 0) {
        txEventJson["remoteStartId"] = txEvent.remoteStartId;
    }

    if (serializeTransactionEventChargingState(txEvent.chargingState)) { // optional
        txEventJson["chargingState"] = serializeTransactionEventChargingState(txEvent.chargingState);
    }

    if (txEvent.idToken) {
        JsonObject idToken = txEventJson.createNestedObject("idToken");
        idToken["idToken"] = txEvent.idToken->get();
        idToken["type"] = txEvent.idToken->getTypeCstr();
    }

    if (txEvent.evse.id >= 0) {
        JsonObject evse = txEventJson.createNestedObject("evse");
        evse["id"] = txEvent.evse.id;
        if (txEvent.evse.connectorId >= 0) {
            evse["connectorId"] = txEvent.evse.connectorId;
        }
    }

    //meterValue not supported yet

    txEventJson["opNr"] = txEvent.opNr;
    txEventJson["attemptNr"] = txEvent.attemptNr;
    
    if (txEvent.attemptTime.isDefined()) {
        char timeStr [MO_JSONDATE_SIZE] = {'\0'};
        if (!context.getClock().toInternalString(txEvent.attemptTime, timeStr, sizeof(timeStr))) {
            return false;
        }
        txEventJson["attemptTime"] = timeStr;
    }

    return true;
}

bool Ocpp201::TransactionStoreEvse::deserializeTransactionEvent(TransactionEventData& txEvent, JsonObject txEventJson) {

    TransactionEventData::Type eventType;
    if (!deserializeTransactionEventType(txEventJson["eventType"] | "Updated", eventType)) {
        return false;
    }
    txEvent.eventType = eventType;

    if (txEventJson.containsKey("timestamp")) {
        if (!context.getClock().parseString(txEventJson["timestamp"] | "_Undefined", txEvent.timestamp)) {
            return false;
        }
    }

    MO_TxEventTriggerReason triggerReason;
    if (!deserializeTxEventTriggerReason(txEventJson["triggerReason"] | "_Undefined", triggerReason)) {
        return false;
    }
    txEvent.triggerReason = triggerReason;
    
    if (txEventJson.containsKey("offline") && !txEventJson["offline"].is<bool>()) {
        return false;
    }
    txEvent.offline = txEventJson["offline"] | false;

    if (txEventJson.containsKey("numberOfPhasesUsed")) {
        int numberOfPhasesUsedIn = txEventJson["numberOfPhasesUsed"] | -1;
        if (numberOfPhasesUsedIn < 0) {
            return false;
        }
        txEvent.numberOfPhasesUsed = numberOfPhasesUsedIn;
    }

    if (txEventJson.containsKey("cableMaxCurrent")) {
        int cableMaxCurrentIn = txEventJson["cableMaxCurrent"] | -1;
        if (cableMaxCurrentIn < 0) {
            return false;
        }
        txEvent.cableMaxCurrent = cableMaxCurrentIn;
    }

    if (txEventJson.containsKey("reservationId")) {
        int reservationIdIn = txEventJson["reservationId"] | -1;
        if (reservationIdIn < 0) {
            return false;
        }
        txEvent.reservationId = reservationIdIn;
    }

    if (txEventJson.containsKey("remoteStartId")) {
        int remoteStartIdIn = txEventJson["remoteStartId"] | -1;
        if (remoteStartIdIn < 0) {
            return false;
        }
        txEvent.remoteStartId = remoteStartIdIn;
    }

    TransactionEventData::ChargingState chargingState;
    if (!deserializeTransactionEventChargingState(txEventJson["chargingState"] | (const char*)nullptr, chargingState)) {
        return false;
    }
    txEvent.chargingState = chargingState;

    if (txEventJson.containsKey("idToken")) {
        auto idToken = std::unique_ptr<IdToken>(new IdToken());
        if (!idToken) {
            MO_DBG_ERR("OOM");
            return false;
        }
        if (!idToken->parseCstr(
                    txEventJson["idToken"]["idToken"] | (const char*)nullptr, 
                    txEventJson["idToken"]["type"]    | (const char*)nullptr)) {
            return false;
        }
        txEvent.idToken = std::move(idToken);
    }

    if (txEventJson.containsKey("evse")) {
        int evseId = txEventJson["evse"]["id"] | -1;
        if (evseId < 0) {
            return false;
        }
        if (txEventJson["evse"].containsKey("connectorId")) {
            int connectorId = txEventJson["evse"]["connectorId"] | -1;
            if (connectorId < 0) {
                return false;
            }
            txEvent.evse = EvseId(evseId, connectorId);
        } else {
            txEvent.evse = EvseId(evseId);
        }
    }

    //meterValue not supported yet

    int opNrIn = txEventJson["opNr"] | -1;
    if (opNrIn >= 0) {
        txEvent.opNr = (unsigned int)opNrIn;
    } else {
        return false;
    }

    int attemptNrIn = txEventJson["attemptNr"] | -1;
    if (attemptNrIn >= 0) {
        txEvent.attemptNr = (unsigned int)attemptNrIn;
    } else {
        return false;
    }

    if (txEventJson.containsKey("attemptTime")) {
        if (!context.getClock().parseString(txEventJson["attemptTime"] | "_Undefined", txEvent.attemptTime)) {
            return false;
        }
    }

    return true;
}

Ocpp201::TransactionStoreEvse::TransactionStoreEvse(Context& context, unsigned int evseId) :
        MemoryManaged("v201.Transactions.TransactionStore"),
        context(context),
        evseId(evseId) {

}

bool Ocpp201::TransactionStoreEvse::setup() {
    filesystem = context.getFilesystem();
    if (!filesystem) {
        MO_DBG_DEBUG("volatile mode");
    }
    return true;
}

bool Ocpp201::TransactionStoreEvse::printTxEventFname(char *fname, size_t size, unsigned int evseId, unsigned int txNr, unsigned int seqNo) {
    auto ret = snprintf(fname, size, "tx201-%.*u-%.*u-%.*u.json",
            MO_NUM_EVSEID_DIGITS, evseId, MO_TXNR_DIGITS, txNr, MO_TXEVENTRECORD_DIGITS, seqNo);
    if (ret < 0 || (size_t)ret >= size) {
        MO_DBG_ERR("fn error: %i", ret);
        return false;
    }
    return true;
}

namespace MicroOcpp {

struct LoadSeqNosData {
    const char *fnPrefix = nullptr;
    size_t fnPrefixLen = 0;
    Vector<unsigned int> *seqNos = nullptr;
};

int loadSeqNoEntry(const char *fname, void* user_data) {
    auto data = reinterpret_cast<LoadSeqNosData*>(user_data);
    if (!strncmp(fname, data->fnPrefix, data->fnPrefixLen)) {
        unsigned int parsedSeqNo = 0;
        for (size_t i = data->fnPrefixLen; fname[i] >= '0' && fname[i] <= '9'; i++) {
            parsedSeqNo *= 10;
            parsedSeqNo += fname[i] - '0';
        }

        data->seqNos->push_back(parsedSeqNo);
    }
    return 0;
}
} //namespace MicroOcpp

std::unique_ptr<Ocpp201::Transaction> Ocpp201::TransactionStoreEvse::loadTransaction(unsigned int txNr) {

    if (!filesystem) {
        MO_DBG_DEBUG("no FS adapter");
        return nullptr;
    }

    char fnPrefix [MO_MAX_PATH_SIZE];
    auto ret = snprintf(fnPrefix, sizeof(fnPrefix), "tx201-%.*u-%.*u-",
            MO_NUM_EVSEID_DIGITS, evseId, MO_TXNR_DIGITS, txNr);
    if (ret < 0 || (size_t)ret >= sizeof(fnPrefix)) {
        MO_DBG_ERR("fn error");
        return nullptr;
    }
    size_t fnPrefixLen = strlen(fnPrefix);

    Vector<unsigned int> seqNos = makeVector<unsigned int>(getMemoryTag());

    LoadSeqNosData data;
    data.fnPrefix = fnPrefix;
    data.fnPrefixLen = fnPrefixLen;
    data.seqNos = &seqNos;

    filesystem->ftw(filesystem->path_prefix, loadSeqNoEntry, reinterpret_cast<void*>(&data));

    if (seqNos.empty()) {
        MO_DBG_DEBUG("no tx at tx201-%u-%u", evseId, txNr);
        return nullptr;
    }

    std::sort(seqNos.begin(), seqNos.end());

    char fn [MO_MAX_PATH_SIZE] = {'\0'};
    if (!printTxEventFname(fn, sizeof(fn), evseId, txNr, seqNos.back())) {
        MO_DBG_ERR("fn error: %i", ret);
        return nullptr;
    }

    JsonDoc doc (0);
    auto status = FilesystemUtils::loadJson(filesystem, fn, doc, getMemoryTag());
    switch (status) {
        case FilesystemUtils::LoadStatus::Success:
            break; //continue loading JSON
        case FilesystemUtils::LoadStatus::FileNotFound:
            MO_DBG_ERR("memory corruption");
            return nullptr;
        case FilesystemUtils::LoadStatus::ErrOOM:
            MO_DBG_ERR("OOM");
            return nullptr;
        case FilesystemUtils::LoadStatus::ErrFileCorruption:
        case FilesystemUtils::LoadStatus::ErrOther:
            MO_DBG_ERR("failed to load %s", fn);
            return nullptr;
    }

    auto transaction = std::unique_ptr<Transaction>(new Transaction(context.getClock()));
    if (!transaction) {
        MO_DBG_ERR("OOM");
        return nullptr;
    }

    transaction->evseId = evseId;
    transaction->txNr = txNr;
    transaction->seqNos = std::move(seqNos);

    JsonObject txJson = doc["tx"];

    if (!deserializeTransaction(*transaction, txJson)) {
        MO_DBG_ERR("deserialization error");
        return nullptr;
    }

    //determine seqNoEnd and trim seqNos record
    if (doc.containsKey("txEvent")) {
        //last tx201 file contains txEvent -> txNoEnd is one place after tx201 file and seqNos is accurate
        transaction->seqNoEnd = transaction->seqNos.back() + 1;
    } else {
        //last tx201 file contains only tx status information, but no txEvent -> remove from seqNos record and set seqNoEnd to this
        transaction->seqNoEnd = transaction->seqNos.back();
        transaction->seqNos.pop_back();
    }

    MO_DBG_DEBUG("loaded tx %u-%u, seqNos.size()=%zu", evseId, txNr, transaction->seqNos.size());

    return transaction;
}

std::unique_ptr<Ocpp201::Transaction> Ocpp201::TransactionStoreEvse::createTransaction(unsigned int txNr, const char *txId) {

    //clean data which could still be here from a rolled-back transaction
    if (!remove(txNr)) {
        MO_DBG_ERR("txNr not clean");
        return nullptr;
    }

    auto transaction = std::unique_ptr<Transaction>(new Transaction(context.getClock()));
    if (!transaction) {
        MO_DBG_ERR("OOM");
        return nullptr;
    }

    transaction->evseId = evseId;
    transaction->txNr = txNr;

    auto ret = snprintf(transaction->transactionId, sizeof(transaction->transactionId), "%s", txId);
    if (ret < 0 || (size_t)ret >= sizeof(transaction->transactionId)) {
        MO_DBG_ERR("invalid arg");
        return nullptr;
    }

    if (!commit(transaction.get())) {
        MO_DBG_ERR("FS error");
        return nullptr;
    }

    return transaction;
}

std::unique_ptr<Ocpp201::TransactionEventData> Ocpp201::TransactionStoreEvse::createTransactionEvent(Transaction& tx) {

    auto txEvent = std::unique_ptr<TransactionEventData>(new TransactionEventData(&tx, tx.seqNoEnd));
    if (!txEvent) {
        MO_DBG_ERR("OOM");
        return nullptr;
    }

    //success
    return txEvent;
}

std::unique_ptr<Ocpp201::TransactionEventData> Ocpp201::TransactionStoreEvse::loadTransactionEvent(Transaction& tx, unsigned int seqNo) {

    if (!filesystem) {
        MO_DBG_DEBUG("no FS adapter");
        return nullptr;
    }

    bool found = false;
    for (size_t i = 0; i < tx.seqNos.size(); i++) {
        if (tx.seqNos[i] == seqNo) {
            found = true;
        }
    }
    if (!found) {
        MO_DBG_DEBUG("%u-%u-%u does not exist", evseId, tx.txNr, seqNo);
        return nullptr;
    }

    char fn [MO_MAX_PATH_SIZE];
    if (!printTxEventFname(fn, sizeof(fn), evseId, tx.txNr, seqNo)) {
        MO_DBG_ERR("fn error: %s", fn);
        return nullptr;
    }

    JsonDoc doc (0);
    auto ret = FilesystemUtils::loadJson(filesystem, fn, doc, getMemoryTag());
    switch (ret) {
        case FilesystemUtils::LoadStatus::Success:
            break; //continue loading JSON
        case FilesystemUtils::LoadStatus::FileNotFound:
            MO_DBG_ERR("seqNos out of sync: could not find %u-%u-%u", evseId, tx.txNr, seqNo);
            return nullptr;
        case FilesystemUtils::LoadStatus::ErrOOM:
            MO_DBG_ERR("OOM");
            return nullptr;
        case FilesystemUtils::LoadStatus::ErrFileCorruption:
        case FilesystemUtils::LoadStatus::ErrOther:
            MO_DBG_ERR("failed to load %s", fn);
            return nullptr;
    }

    if (!doc.containsKey("txEvent")) {
        MO_DBG_DEBUG("%u-%u-%u does not contain txEvent", evseId, tx.txNr, seqNo);
        return nullptr;
    }

    auto txEvent = std::unique_ptr<TransactionEventData>(new TransactionEventData(&tx, seqNo));
    if (!txEvent) {
        MO_DBG_ERR("OOM");
        return nullptr;
    }

    if (!deserializeTransactionEvent(*txEvent, doc["txEvent"])) {
        MO_DBG_ERR("deserialization error");
        return nullptr;
    }

    return txEvent;
}

bool Ocpp201::TransactionStoreEvse::commit(Transaction& tx, TransactionEventData *txEvent) {

    if (!filesystem) {
        MO_DBG_DEBUG("no FS: nothing to commit");
        return true;
    }

    unsigned int seqNo = 0;

    if (txEvent) {
        seqNo = txEvent->seqNo;
    } else {
        //update tx state in new or reused tx201 file
        seqNo = tx.seqNoEnd;
    }

    size_t seqNosNewSize = tx.seqNos.size() + 1;
    for (size_t i = 0; i < tx.seqNos.size(); i++) {
        if (tx.seqNos[i] == seqNo) {
            seqNosNewSize -= 1;
            break;
        }
    }

    // Check if to delete intermediate offline txEvent
    if (seqNosNewSize > MO_TXEVENTRECORD_SIZE) {
        auto deltaMin = std::numeric_limits<unsigned int>::max();
        size_t indexMin = tx.seqNos.size();
        for (size_t i = 2; i + 1 <= tx.seqNos.size(); i++) { //always keep first and final txEvent
            size_t t0 = tx.seqNos.size() - i - 1;
            size_t t1 = tx.seqNos.size() - i;
            size_t t2 = tx.seqNos.size() - i + 1;

            auto delta = tx.seqNos[t2] - tx.seqNos[t0];

            if (delta < deltaMin) {
                deltaMin = delta;
                indexMin = t1;
            }
        }

        if (indexMin < tx.seqNos.size()) {
            MO_DBG_DEBUG("delete intermediate txEvent %u-%u-%u - delta=%u", evseId, tx.txNr, tx.seqNos[indexMin], deltaMin);
            remove(tx, tx.seqNos[indexMin]); //remove can call commit() again. Ensure that remove is not executed for last element
        } else {
            MO_DBG_ERR("internal error");
            return false;
        }
    }

    char fn [MO_MAX_PATH_SIZE];
    if (!printTxEventFname(fn, sizeof(fn), evseId, tx.txNr, seqNo)) {
        MO_DBG_ERR("fn error: %s", fn);
        return false;
    }

    auto txDoc = initJsonDoc("v201.Transactions.TransactionStoreEvse", 2048);

    if (!serializeTransaction(tx, txDoc.createNestedObject("tx"))) {
        MO_DBG_ERR("Serialization error");
        return false;
    }

    if (txEvent && !serializeTransactionEvent(*txEvent, txDoc.createNestedObject("txEvent"))) {
        MO_DBG_ERR("Serialization error");
        return false;
    }

    if (FilesystemUtils::storeJson(filesystem, fn, txDoc) != FilesystemUtils::StoreStatus::Success) {
        MO_DBG_ERR("FS error");
        return false;
    }

    if (txEvent && seqNo == tx.seqNoEnd) {
        tx.seqNos.push_back(seqNo);
        tx.seqNoEnd++;
    }

    MO_DBG_DEBUG("comitted tx %u-%u-%u", evseId, tx.txNr, seqNo);

    //success
    return true;
}

bool Ocpp201::TransactionStoreEvse::commit(Transaction *transaction) {
    return commit(*transaction, nullptr);
}

bool Ocpp201::TransactionStoreEvse::commit(TransactionEventData *txEvent) {
    return commit(*txEvent->transaction, txEvent);
}

bool Ocpp201::TransactionStoreEvse::remove(unsigned int txNr) {

    if (!filesystem) {
        MO_DBG_DEBUG("no FS: nothing to remove");
        return true;
    }

    char fnPrefix [MO_MAX_PATH_SIZE];
    auto ret = snprintf(fnPrefix, sizeof(fnPrefix), "tx201-%.*u-%.*u-",
            MO_NUM_EVSEID_DIGITS, evseId, MO_TXNR_DIGITS, txNr);
    if (ret < 0 || (size_t)ret >= sizeof(fnPrefix)) {
        MO_DBG_ERR("fn error");
        return false;
    }

    return FilesystemUtils::removeByPrefix(filesystem, fnPrefix);
}

bool Ocpp201::TransactionStoreEvse::remove(Transaction& tx, unsigned int seqNo) {

    if (tx.seqNos.empty()) {
        //nothing to do
        return true;
    }

    if (tx.seqNos.back() == seqNo) {
        //special case: deletion of last tx201 file could also delete information about tx. Make sure all tx-related
        //information is commited into tx201 file at seqNoEnd, then delete file at seqNo

        char fn [MO_MAX_PATH_SIZE];
        if (!printTxEventFname(fn, sizeof(fn), evseId, tx.txNr, tx.seqNoEnd)) {
            MO_DBG_ERR("fn error");
            return false;
        }

        JsonDoc doc (0);
        auto ret = FilesystemUtils::loadJson(filesystem, fn, doc, getMemoryTag());
        switch (ret) {
            case FilesystemUtils::LoadStatus::Success:
                break; //continue loading JSON
            case FilesystemUtils::LoadStatus::FileNotFound:
                break;
            case FilesystemUtils::LoadStatus::ErrOOM:
                MO_DBG_ERR("OOM");
                return false;
            case FilesystemUtils::LoadStatus::ErrFileCorruption:
            case FilesystemUtils::LoadStatus::ErrOther:
                MO_DBG_ERR("failed to load %s", fn);
                break;
        }

        if (ret != FilesystemUtils::LoadStatus::Success || !doc.containsKey("tx")) {
            //no valid tx201 file at seqNoEnd. Commit tx into file seqNoEnd, then remove file at seqNo

            if (!commit(tx, nullptr)) {
                MO_DBG_ERR("fs error");
                return false;
            }
        }

        //seqNoEnd contains all tx data which should be persisted. Continue
    }

    bool found = false;
    for (size_t i = 0; i < tx.seqNos.size(); i++) {
        if (tx.seqNos[i] == seqNo) {
            found = true;
            break;
        }
    }
    if (!found) {
        MO_DBG_DEBUG("%u-%u-%u does not exist", evseId, tx.txNr, seqNo);
        return true;
    }

    bool success = true;

    if (filesystem) {
        char fn [MO_MAX_PATH_SIZE];
        if (!printTxEventFname(fn, sizeof(fn), evseId, tx.txNr, seqNo)) {
            MO_DBG_ERR("fn error: %s", fn);
            return false;
        }

        char path [MO_MAX_PATH_SIZE];
        if (!FilesystemUtils::printPath(filesystem, path, sizeof(path), fn)) {
            MO_DBG_ERR("path error: %s", fn);
            return false;
        }

        size_t msize;
        if (filesystem->stat(path, &msize) == 0) {
            success &= filesystem->remove(path);
        } else {
            MO_DBG_ERR("internal error: seqNos out of sync");
            (void)0;
        }
    }

    if (success) {
        auto it = tx.seqNos.begin();
        while (it != tx.seqNos.end()) {
            if (*it == seqNo) {
                it = tx.seqNos.erase(it);
            } else {
                it++;
            }
        }
    }

    return success;
}

Ocpp201::TransactionStore::TransactionStore(Context& context) :
        MemoryManaged{"v201.Transactions.TransactionStore"}, context(context) {

}

Ocpp201::TransactionStore::~TransactionStore() {
    for (unsigned int evseId = 0; evseId < numEvseId; evseId++) {
        delete evses[evseId];
    }
}

bool Ocpp201::TransactionStore::setup() {

    numEvseId = context.getModel201().getNumEvseId();
    for (unsigned int i = 0; i < numEvseId; i++) {
        if (!getEvse(i) || !getEvse(i)->setup()) {
            MO_DBG_ERR("initialization error");
            return false;
        }
    }

    return true;
}

Ocpp201::TransactionStoreEvse *Ocpp201::TransactionStore::getEvse(unsigned int evseId) {
    if (evseId >= numEvseId) {
        MO_DBG_ERR("evseId out of bound");
        return nullptr;
    }

    if (!evses[evseId]) {
        evses[evseId] = new TransactionStoreEvse(context, evseId);
        if (!evses[evseId]) {
            MO_DBG_ERR("OOM");
            return nullptr;
        }
    }

    return evses[evseId];
}

#endif //MO_ENABLE_V201
