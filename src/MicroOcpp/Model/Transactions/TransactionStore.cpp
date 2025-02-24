// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Model/Transactions/TransactionDeserialize.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

TransactionStoreEvse::TransactionStoreEvse(TransactionStore& context, unsigned int connectorId, std::shared_ptr<FilesystemAdapter> filesystem) :
        MemoryManaged("v16.Transactions.TransactionStore"),
        context(context),
        connectorId(connectorId),
        filesystem(filesystem),
        transactions{makeVector<std::weak_ptr<Transaction>>(getMemoryTag())} {

}

TransactionStoreEvse::~TransactionStoreEvse() {

}

std::shared_ptr<Transaction> TransactionStoreEvse::getTransaction(unsigned int txNr) {

    //check for most recent element of cache first because of temporal locality
    if (!transactions.empty()) {
        if (auto cached = transactions.back().lock()) {
            if (cached->getTxNr() == txNr) {
                //cache hit
                return cached;
            }
        }
    }

    //check all other elements (and free up unused entries)
    auto cached = transactions.begin();
    while (cached != transactions.end()) {
        if (auto tx = cached->lock()) {
            if (tx->getTxNr() == txNr) {
                //cache hit
                return tx;
            }
            cached++;
        } else {
            //collect outdated cache reference
            cached = transactions.erase(cached);
        }
    }

    //cache miss - load tx from flash if existent
    
    if (!filesystem) {
        MO_DBG_DEBUG("no FS adapter");
        return nullptr;
    }

    char fn [MO_MAX_PATH_SIZE] = {'\0'};
    auto ret = snprintf(fn, MO_MAX_PATH_SIZE, MO_FILENAME_PREFIX "tx" "-%u-%u.json", connectorId, txNr);
    if (ret < 0 || ret >= MO_MAX_PATH_SIZE) {
        MO_DBG_ERR("fn error: %i", ret);
        return nullptr;
    }

    size_t msize;
    if (filesystem->stat(fn, &msize) != 0) {
        MO_DBG_DEBUG("%u-%u does not exist", connectorId, txNr);
        return nullptr;
    }

    auto doc = FilesystemUtils::loadJson(filesystem, fn, getMemoryTag());

    if (!doc) {
        MO_DBG_ERR("memory corruption");
        return nullptr;
    }

    auto transaction = std::allocate_shared<Transaction>(makeAllocator<Transaction>(getMemoryTag()), *this, connectorId, txNr);
    JsonObject txJson = doc->as<JsonObject>();
    if (!deserializeTransaction(*transaction, txJson)) {
        MO_DBG_ERR("deserialization error");
        return nullptr;
    }

    //before adding new entry, clean cache
    cached = transactions.begin();
    while (cached != transactions.end()) {
        if (cached->expired()) {
            //collect outdated cache reference
            cached = transactions.erase(cached);
        } else {
            cached++;
        }
    }

    transactions.push_back(transaction);
    return transaction;
}

std::shared_ptr<Transaction> TransactionStoreEvse::createTransaction(unsigned int txNr, bool silent) {

    auto transaction = std::allocate_shared<Transaction>(makeAllocator<Transaction>(getMemoryTag()), *this, connectorId, txNr, silent);

    if (!commit(transaction.get())) {
        MO_DBG_ERR("FS error");
        return nullptr;
    }

    //before adding new entry, clean cache
    auto cached = transactions.begin();
    while (cached != transactions.end()) {
        if (cached->expired()) {
            //collect outdated cache reference
            cached = transactions.erase(cached);
        } else {
            cached++;
        }
    }

    transactions.push_back(transaction);
    return transaction;
}

bool TransactionStoreEvse::commit(Transaction *transaction) {

    if (!filesystem) {
        MO_DBG_DEBUG("no FS: nothing to commit");
        return true;
    }

    char fn [MO_MAX_PATH_SIZE] = {'\0'};
    auto ret = snprintf(fn, MO_MAX_PATH_SIZE, MO_FILENAME_PREFIX "tx" "-%u-%u.json", connectorId, transaction->getTxNr());
    if (ret < 0 || ret >= MO_MAX_PATH_SIZE) {
        MO_DBG_ERR("fn error: %i", ret);
        return false;
    }
    
    auto txDoc = initJsonDoc(getMemoryTag());
    if (!serializeTransaction(*transaction, txDoc)) {
        MO_DBG_ERR("Serialization error");
        return false;
    }

    if (!FilesystemUtils::storeJson(filesystem, fn, txDoc)) {
        MO_DBG_ERR("FS error");
        return false;
    }

    //success
    return true;
}

bool TransactionStoreEvse::remove(unsigned int txNr) {

    if (!filesystem) {
        MO_DBG_DEBUG("no FS: nothing to remove");
        return true;
    }

    char fn [MO_MAX_PATH_SIZE] = {'\0'};
    auto ret = snprintf(fn, MO_MAX_PATH_SIZE, MO_FILENAME_PREFIX "tx" "-%u-%u.json", connectorId, txNr);
    if (ret < 0 || ret >= MO_MAX_PATH_SIZE) {
        MO_DBG_ERR("fn error: %i", ret);
        return false;
    }

    size_t msize;
    if (filesystem->stat(fn, &msize) != 0) {
        MO_DBG_DEBUG("%s already removed", fn);
        return true;
    }

    MO_DBG_DEBUG("remove %s", fn);
    
    return filesystem->remove(fn);
}

#if MO_ENABLE_V201

#include <algorithm>

namespace MicroOcpp {
namespace Ocpp201 {

bool TransactionStoreEvse::serializeTransaction(Transaction& tx, JsonObject txJson) {

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

    if (tx.beginTimestamp > MIN_TIME) {
        char timeStr [JSONDATE_LENGTH + 1] = {'\0'};
        tx.beginTimestamp.toJsonString(timeStr, JSONDATE_LENGTH + 1);
        txJson["beginTimestamp"] = timeStr;
    }

    if (tx.remoteStartId >= 0) {
        txJson["remoteStartId"] = tx.remoteStartId;
    }

    if (tx.evConnectionTimeoutListen) {
        txJson["evConnectionTimeoutListen"] = true;
    }

    if (serializeTransactionStoppedReason(tx.stoppedReason)) { // optional
        txJson["stoppedReason"] = serializeTransactionStoppedReason(tx.stoppedReason);
    }

    if (serializeTransactionEventTriggerReason(tx.stopTrigger)) {
        txJson["stopTrigger"] = serializeTransactionEventTriggerReason(tx.stopTrigger);
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

bool TransactionStoreEvse::deserializeTransaction(Transaction& tx, JsonObject txJson) {

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
        if (!tx.beginTimestamp.setTime(txJson["beginTimestamp"] | "_Undefined")) {
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

    if (txJson.containsKey("evConnectionTimeoutListen") && !txJson["evConnectionTimeoutListen"].is<bool>()) {
        return false;
    }
    tx.evConnectionTimeoutListen = txJson["evConnectionTimeoutListen"] | false;

    Transaction::StoppedReason stoppedReason;
    if (!deserializeTransactionStoppedReason(txJson["stoppedReason"] | (const char*)nullptr, stoppedReason)) {
        return false;
    }
    tx.stoppedReason = stoppedReason;

    TransactionEventTriggerReason stopTrigger;
    if (!deserializeTransactionEventTriggerReason(txJson["stopTrigger"] | (const char*)nullptr, stopTrigger)) {
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

bool TransactionStoreEvse::serializeTransactionEvent(TransactionEventData& txEvent, JsonObject txEventJson) {
    
    if (txEvent.eventType != TransactionEventData::Type::Updated) {
        txEventJson["eventType"] = serializeTransactionEventType(txEvent.eventType);
    }

    if (txEvent.timestamp > MIN_TIME) {
        char timeStr [JSONDATE_LENGTH + 1] = {'\0'};
        txEvent.timestamp.toJsonString(timeStr, JSONDATE_LENGTH + 1);
        txEventJson["timestamp"] = timeStr;
    }

    txEventJson["bootNr"] = txEvent.bootNr;

    if (serializeTransactionEventTriggerReason(txEvent.triggerReason)) {
        txEventJson["triggerReason"] = serializeTransactionEventTriggerReason(txEvent.triggerReason);
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
    
    if (txEvent.attemptTime > MIN_TIME) {
        char timeStr [JSONDATE_LENGTH + 1] = {'\0'};
        txEvent.attemptTime.toJsonString(timeStr, JSONDATE_LENGTH + 1);
        txEventJson["attemptTime"] = timeStr;
    }

    return true;
}

bool TransactionStoreEvse::deserializeTransactionEvent(TransactionEventData& txEvent, JsonObject txEventJson) {

    TransactionEventData::Type eventType;
    if (!deserializeTransactionEventType(txEventJson["eventType"] | "Updated", eventType)) {
        return false;
    }
    txEvent.eventType = eventType;

    if (txEventJson.containsKey("timestamp")) {
        if (!txEvent.timestamp.setTime(txEventJson["timestamp"] | "_Undefined")) {
            return false;
        }
    }

    int bootNrIn = txEventJson["bootNr"] | -1;
    if (bootNrIn >= 0 && bootNrIn <= std::numeric_limits<uint16_t>::max()) {
        txEvent.bootNr = (uint16_t)bootNrIn;
    } else {
        return false;
    }

    TransactionEventTriggerReason triggerReason;
    if (!deserializeTransactionEventTriggerReason(txEventJson["triggerReason"] | "_Undefined", triggerReason)) {
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
        if (!txEvent.attemptTime.setTime(txEventJson["attemptTime"] | "_Undefined")) {
            return false;
        }
    }

    return true;
}

TransactionStoreEvse::TransactionStoreEvse(TransactionStore& txStore, unsigned int evseId, std::shared_ptr<FilesystemAdapter> filesystem) :
        MemoryManaged("v201.Transactions.TransactionStore"),
        txStore(txStore),
        evseId(evseId),
        filesystem(filesystem) {

}

bool TransactionStoreEvse::discoverStoredTx(unsigned int& txNrBeginOut, unsigned int& txNrEndOut) {

    if (!filesystem) {
        MO_DBG_DEBUG("no FS adapter");
        return true;
    }

    char fnPrefix [MO_MAX_PATH_SIZE];
    snprintf(fnPrefix, sizeof(fnPrefix), "tx201-%u-", evseId);
    size_t fnPrefixLen = strlen(fnPrefix);

    unsigned int txNrPivot = std::numeric_limits<unsigned int>::max();
    unsigned int txNrBegin = 0, txNrEnd = 0;

    auto ret = filesystem->ftw_root([fnPrefix, fnPrefixLen, &txNrPivot, &txNrBegin, &txNrEnd] (const char *fn) {
        if (!strncmp(fn, fnPrefix, fnPrefixLen)) {
            unsigned int parsedTxNr = 0;
            for (size_t i = fnPrefixLen; fn[i] >= '0' && fn[i] <= '9'; i++) {
                parsedTxNr *= 10;
                parsedTxNr += fn[i] - '0';
            }

            if (txNrPivot == std::numeric_limits<unsigned int>::max()) {
                txNrPivot = parsedTxNr;
                txNrBegin = parsedTxNr;
                txNrEnd = (parsedTxNr + 1) % MAX_TX_CNT;
                return 0;
            }

            if ((parsedTxNr + MAX_TX_CNT - txNrPivot) % MAX_TX_CNT < MAX_TX_CNT / 2) {
                //parsedTxNr is after pivot point
                if ((parsedTxNr + 1 + MAX_TX_CNT - txNrPivot) % MAX_TX_CNT > (txNrEnd + MAX_TX_CNT - txNrPivot) % MAX_TX_CNT) {
                    txNrEnd = (parsedTxNr + 1) % MAX_TX_CNT;
                }
            } else if ((txNrPivot + MAX_TX_CNT - parsedTxNr) % MAX_TX_CNT < MAX_TX_CNT / 2) {
                //parsedTxNr is before pivot point
                if ((txNrPivot + MAX_TX_CNT - parsedTxNr) % MAX_TX_CNT > (txNrPivot + MAX_TX_CNT - txNrBegin) % MAX_TX_CNT) {
                    txNrBegin = parsedTxNr;
                }
            }

            MO_DBG_DEBUG("found %s%u-*.json - Internal range from %u to %u (exclusive)", fnPrefix, parsedTxNr, txNrBegin, txNrEnd);
        }
        return 0;
    });

    if (ret == 0) {
        txNrBeginOut = txNrBegin;
        txNrEndOut = txNrEnd;
        return true;
    } else {
        MO_DBG_ERR("fs error");
        return false;
    }
}

std::unique_ptr<Transaction> TransactionStoreEvse::loadTransaction(unsigned int txNr) {

    if (!filesystem) {
        MO_DBG_DEBUG("no FS adapter");
        return nullptr;
    }

    char fnPrefix [MO_MAX_PATH_SIZE];
    auto ret= snprintf(fnPrefix, sizeof(fnPrefix), "tx201-%u-%u-", evseId, txNr);
    if (ret < 0 || (size_t)ret >= sizeof(fnPrefix)) {
        MO_DBG_ERR("fn error");
        return nullptr;
    }
    size_t fnPrefixLen = strlen(fnPrefix);

    Vector<unsigned int> seqNos = makeVector<unsigned int>(getMemoryTag());

    filesystem->ftw_root([fnPrefix, fnPrefixLen, &seqNos] (const char *fn) {
        if (!strncmp(fn, fnPrefix, fnPrefixLen)) {
            unsigned int parsedSeqNo = 0;
            for (size_t i = fnPrefixLen; fn[i] >= '0' && fn[i] <= '9'; i++) {
                parsedSeqNo *= 10;
                parsedSeqNo += fn[i] - '0';
            }

            seqNos.push_back(parsedSeqNo);
        }
        return 0;
    });

    if (seqNos.empty()) {
        MO_DBG_DEBUG("no tx at tx201-%u-%u", evseId, txNr);
        return nullptr;
    }

    std::sort(seqNos.begin(), seqNos.end());

    char fn [MO_MAX_PATH_SIZE] = {'\0'};
    ret = snprintf(fn, MO_MAX_PATH_SIZE, MO_FILENAME_PREFIX "tx201" "-%u-%u-%u.json", evseId, txNr, seqNos.back());
    if (ret < 0 || ret >= MO_MAX_PATH_SIZE) {
        MO_DBG_ERR("fn error: %i", ret);
        return nullptr;
    }

    size_t msize;
    if (filesystem->stat(fn, &msize) != 0) {
        MO_DBG_ERR("tx201-%u-%u memory corruption", evseId, txNr);
        return nullptr;
    }

    auto doc = FilesystemUtils::loadJson(filesystem, fn, getMemoryTag());

    if (!doc) {
        MO_DBG_ERR("memory corruption");
        return nullptr;
    }

    auto transaction = std::unique_ptr<Transaction>(new Transaction());
    if (!transaction) {
        MO_DBG_ERR("OOM");
        return nullptr;
    }

    transaction->evseId = evseId;
    transaction->txNr = txNr;
    transaction->seqNos = std::move(seqNos);

    JsonObject txJson = (*doc)["tx"];

    if (!deserializeTransaction(*transaction, txJson)) {
        MO_DBG_ERR("deserialization error");
        return nullptr;
    }

    //determine seqNoEnd and trim seqNos record
    if (doc->containsKey("txEvent")) {
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

std::unique_ptr<Ocpp201::Transaction> TransactionStoreEvse::createTransaction(unsigned int txNr, const char *txId) {

    //clean data which could still be here from a rolled-back transaction
    if (!remove(txNr)) {
        MO_DBG_ERR("txNr not clean");
        return nullptr;
    }

    auto transaction = std::unique_ptr<Transaction>(new Transaction());
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

std::unique_ptr<TransactionEventData> TransactionStoreEvse::createTransactionEvent(Transaction& tx) {

    auto txEvent = std::unique_ptr<TransactionEventData>(new TransactionEventData(&tx, tx.seqNoEnd));
    if (!txEvent) {
        MO_DBG_ERR("OOM");
        return nullptr;
    }

    //success
    return txEvent;
}

std::unique_ptr<TransactionEventData> TransactionStoreEvse::loadTransactionEvent(Transaction& tx, unsigned int seqNo) {

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

    char fn [MO_MAX_PATH_SIZE] = {'\0'};
    auto ret = snprintf(fn, MO_MAX_PATH_SIZE, MO_FILENAME_PREFIX "tx201" "-%u-%u-%u.json", evseId, tx.txNr, seqNo);
    if (ret < 0 || ret >= MO_MAX_PATH_SIZE) {
        MO_DBG_ERR("fn error: %i", ret);
        return nullptr;
    }

    size_t msize;
    if (filesystem->stat(fn, &msize) != 0) {
        MO_DBG_ERR("seqNos out of sync: could not find %u-%u-%u", evseId, tx.txNr, seqNo);
        return nullptr;
    }

    auto doc = FilesystemUtils::loadJson(filesystem, fn, getMemoryTag());

    if (!doc) {
        MO_DBG_ERR("memory corruption");
        return nullptr;
    }

    if (!doc->containsKey("txEvent")) {
        MO_DBG_DEBUG("%u-%u-%u does not contain txEvent", evseId, tx.txNr, seqNo);
        return nullptr;
    }

    auto txEvent = std::unique_ptr<TransactionEventData>(new TransactionEventData(&tx, seqNo));
    if (!txEvent) {
        MO_DBG_ERR("OOM");
        return nullptr;
    }

    if (!deserializeTransactionEvent(*txEvent, (*doc)["txEvent"])) {
        MO_DBG_ERR("deserialization error");
        return nullptr;
    }

    return txEvent;
}

bool TransactionStoreEvse::commit(Transaction& tx, TransactionEventData *txEvent) {

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
    if (seqNosNewSize > MO_TXEVENTRECORD_SIZE_V201) {
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

    char fn [MO_MAX_PATH_SIZE] = {'\0'};
    auto ret = snprintf(fn, MO_MAX_PATH_SIZE, MO_FILENAME_PREFIX "tx201" "-%u-%u-%u.json", evseId, tx.txNr, seqNo);
    if (ret < 0 || ret >= MO_MAX_PATH_SIZE) {
        MO_DBG_ERR("fn error: %i", ret);
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

    if (!FilesystemUtils::storeJson(filesystem, fn, txDoc)) {
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

bool TransactionStoreEvse::commit(Transaction *transaction) {
    return commit(*transaction, nullptr);
}

bool TransactionStoreEvse::commit(TransactionEventData *txEvent) {
    return commit(*txEvent->transaction, txEvent);
}

bool TransactionStoreEvse::remove(unsigned int txNr) {

    if (!filesystem) {
        MO_DBG_DEBUG("no FS: nothing to remove");
        return true;
    }

    char fnPrefix [MO_MAX_PATH_SIZE];
    auto ret= snprintf(fnPrefix, sizeof(fnPrefix), "tx201-%u-%u-", evseId, txNr);
    if (ret < 0 || (size_t)ret >= sizeof(fnPrefix)) {
        MO_DBG_ERR("fn error");
        return false;
    }
    size_t fnPrefixLen = strlen(fnPrefix);

    auto success = FilesystemUtils::remove_if(filesystem, [fnPrefix, fnPrefixLen] (const char *fn) {
        return !strncmp(fn, fnPrefix, fnPrefixLen);
    });

    return success;
}

bool TransactionStoreEvse::remove(Transaction& tx, unsigned int seqNo) {

    if (tx.seqNos.empty()) {
        //nothing to do
        return true;
    }

    if (tx.seqNos.back() == seqNo) {
        //special case: deletion of last tx201 file could also delete information about tx. Make sure all tx-related
        //information is commited into tx201 file at seqNoEnd, then delete file at seqNo

        char fn [MO_MAX_PATH_SIZE];
        auto ret = snprintf(fn, sizeof(fn), "%stx201-%u-%u-%u.json", MO_FILENAME_PREFIX, evseId, tx.txNr, tx.seqNoEnd);
        if (ret < 0 || (size_t)ret >= sizeof(fn)) {
            MO_DBG_ERR("fn error");
            return false;
        }

        auto doc = FilesystemUtils::loadJson(filesystem, fn, getMemoryTag());
        
        if (!doc || !doc->containsKey("tx")) {
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
        }
    }
    if (!found) {
        MO_DBG_DEBUG("%u-%u-%u does not exist", evseId, tx.txNr, seqNo);
        return true;
    }

    bool success = true;

    if (filesystem) {
        char fn [MO_MAX_PATH_SIZE];
        auto ret = snprintf(fn, sizeof(fn), "%stx201-%u-%u-%u.json", MO_FILENAME_PREFIX, evseId, tx.txNr, seqNo);
        if (ret < 0 || (size_t)ret >= sizeof(fn)) {
            MO_DBG_ERR("fn error");
            return false;
        }

        size_t msize;
        if (filesystem->stat(fn, &msize) == 0) {
            success &= filesystem->remove(fn);
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

TransactionStore::TransactionStore(std::shared_ptr<FilesystemAdapter> filesystem, size_t numEvses) :
        MemoryManaged{"v201.Transactions.TransactionStore"} {

    for (unsigned int evseId = 0; evseId < MO_NUM_EVSEID && (size_t)evseId < numEvses; evseId++) {
        evses[evseId] = new TransactionStoreEvse(*this, evseId, filesystem);
    }
}

TransactionStore::~TransactionStore() {
    for (unsigned int evseId = 0; evseId < MO_NUM_EVSEID && evses[evseId]; evseId++) {
        delete evses[evseId];
    }
}

TransactionStoreEvse *TransactionStore::getEvse(unsigned int evseId) {
    if (evseId >= MO_NUM_EVSEID) {
        return nullptr;
    }
    return evses[evseId];
}

} //namespace Ocpp201
} //namespace MicroOcpp

#endif //MO_ENABLE_V201
