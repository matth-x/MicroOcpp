// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Tasks/Transactions/TransactionStore.h>
#include <ArduinoOcpp/Tasks/Transactions/TransactionService.h>
#include <ArduinoOcpp/Tasks/Transactions/Transaction.h>
#include <ArduinoOcpp/MessagesV16/StartTransaction.h>
#include <ArduinoOcpp/MessagesV16/StopTransaction.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>
#include <ArduinoOcpp/Core/FilesystemUtils.h>
#include <ArduinoOcpp/Debug.h>

#include <algorithm>

using namespace ArduinoOcpp;

#ifndef AO_TXSTORE_DIR
#define AO_TXSTORE_DIR AO_FILENAME_PREFIX "/"
#endif

#define AO_TXSTORE_META_FN AO_FILENAME_PREFIX "/txstore.jsn"

#define MAX_QUEUE_SIZE 20U
#define MAX_TX_CNT 100000U

ConnectorTransactionStore::ConnectorTransactionStore(TransactionService& context, uint connectorId, std::shared_ptr<FilesystemAdapter> filesystem) :
        context(context),
        connectorId(connectorId),
        filesystem(filesystem) {

    char key [30] = {'\0'};
    if (snprintf(key, 30, "AO_txBegin_%u", connectorId) < 0) {
        AO_DBG_ERR("Invalid key");
        (void)0;
    }
    txBegin = declareConfiguration<int>(key, 0, AO_TXSTORE_META_FN, false, false, true, false);
}

void ConnectorTransactionStore::restorePendingTransactions() {

    if (!filesystem) {
        AO_DBG_ERR("FS error");
        return;
    }

    if (!txBegin || *txBegin < 0 || *txBegin > MAX_TX_CNT) {
        AO_DBG_ERR("Invalid state");
        return;
    }

    uint tx = *txBegin;
    txEnd = tx;

    const uint MISSES_LIMIT = 3;
    uint misses = 0;

    while (misses < MISSES_LIMIT) { //search until region without txs found

        char fn [MAX_PATH_SIZE] = {'\0'};
        auto ret = snprintf(fn, MAX_PATH_SIZE, AO_TXSTORE_DIR "tx" "-%u-%u.jsn", connectorId, tx);
        if (ret < 0 || ret >= MAX_PATH_SIZE) {
            AO_DBG_ERR("fn error: %i", ret);
            break; //all files have same length
        }

        auto doc = FilesystemUtils::loadJson(filesystem, fn);

        if (!doc) {
            misses++;
            tx++;
            tx %= MAX_TX_CNT;
            continue;
        }

        std::shared_ptr<Transaction> transaction = std::make_shared<Transaction>(context, connectorId, tx);
        JsonObject txJson = doc->as<JsonObject>();
        if (!transaction->deserializeSessionState(txJson)) {
            AO_DBG_ERR("Deserialization error");
            misses++;
            tx++;
            tx %= MAX_TX_CNT;
            continue;
        }

        if (!transaction->isPending()) {
            AO_DBG_DEBUG("Drop aborted / finished tx");
            tx++;
            tx %= MAX_TX_CNT;
            continue; //normal behavior => don't increment misses
        }

        transactions.push_back(std::move(transaction));

        tx++;
        tx %= MAX_TX_CNT;
        txEnd = tx;
        misses = 0;

        if (transactions.size() > MAX_QUEUE_SIZE) { //allow to exceed MAX_QUEUE_SIZE by 1
            AO_DBG_ERR("txBegin out of sync");
            break;
        }
    }

    AO_DBG_DEBUG("Restored %zu transactions", transactions.size());
}

void ConnectorTransactionStore::submitPendingOperations() {
    for (auto& tx : transactions) {
        
        auto& startRpc = tx->getStartRpcSync();
        if (startRpc.isRequested() && !startRpc.isConfirmed()) {
            //startTx has been initiated, but not been confirmed yet
            AO_DBG_DEBUG("Restore StartTx, connectorId=%i, seqNr=%u", connectorId, startRpc.getSeqNr());

            auto startOp = makeOcppOperation(new Ocpp16::StartTransaction(tx));
            context.addRestoredOperation(startRpc.getSeqNr(), std::move(startOp));
        }

        auto& stopRpc = tx->getStopRpcSync();
        if (stopRpc.isRequested() && !stopRpc.isConfirmed()) {
            //stopTx has been initiated, but not been confirmed yet
            AO_DBG_DEBUG("Restore StopTx, connectorId=%i, seqNr=%u", connectorId, stopRpc.getSeqNr());

            //... add stop data

            auto stopOp = makeOcppOperation(new Ocpp16::StopTransaction(tx));
            context.addRestoredOperation(stopRpc.getSeqNr(), std::move(stopOp));
        }
    }
}

std::shared_ptr<Transaction> ConnectorTransactionStore::makeTransaction() {

    if (!filesystem) {
        AO_DBG_ERR("Need FS adapter");
        //return nullptr; <-- allow test runs without FS
    }

    if (transactions.size() >= MAX_QUEUE_SIZE) {
        AO_DBG_WARN("Queue full");
        return nullptr;
    }

    auto transaction = std::make_shared<Transaction>(context, connectorId, txEnd);
    txEnd++;
    txEnd %= MAX_TX_CNT;

    transactions.push_back(transaction);

    if (!commit(transaction.get())) {
        std::remove_if(transactions.begin(), transactions.end(),
            [&transaction] (const std::shared_ptr<Transaction>& el) {
                return el == transaction;
            }
        );
        txEnd--;
        txEnd += MAX_TX_CNT;
        txEnd %= MAX_TX_CNT;
        return nullptr;
    }

    return transaction;
}

std::shared_ptr<Transaction> ConnectorTransactionStore::getActiveTransaction() {
    
    if (!transactions.empty()) {
        auto& tx = transactions.back();
        if (tx->isActive() || tx->isRunning()) {
            return tx;
        }
    }

    AO_DBG_DEBUG("Make new Tx");

    auto tx = makeTransaction();
    if (tx) {
        tx->setConnectorId(connectorId);
    }

    return tx;
}

std::shared_ptr<Transaction> ConnectorTransactionStore::getTransactionSync() {
    
    if (!transactions.empty()) {
        auto& tx = transactions.front();
        if (tx->isRunning()) {
            return tx;
        }
    }

    return nullptr;
}

bool ConnectorTransactionStore::commit(Transaction *transaction) {
    if (transactions.empty()) {
        AO_DBG_ERR("Dangling transaction");
        return false;
    }

    auto found = transactions.end();
    if (transaction == (transactions.end() - 1)->get()) { //optimization: check if committing the last container element
        found = transactions.end() - 1;
    } else {
        for (auto entry = transactions.begin(); entry != transactions.end(); entry++) {
            if (transaction == entry->get()) {
                found = entry;
                break;
            }
        }
    }

    if (found == transactions.end()) {
        AO_DBG_ERR("Dangling transaction");
        return false;
    }

    //confirmed that transaction points to an element in the transactions container

    char fn [MAX_PATH_SIZE] = {'\0'};
    auto ret = snprintf(fn, MAX_PATH_SIZE, AO_TXSTORE_DIR "tx" "-%u-%u.jsn", connectorId, transaction->getTxNr());
    if (ret < 0 || ret >= MAX_PATH_SIZE) {
        AO_DBG_ERR("fn error: %i", ret);
        return false; //all files have same length
    }
    
    DynamicJsonDocument txDoc {0};
    if (!transaction->serializeSessionState(txDoc)) {
        AO_DBG_ERR("Serialization error");
        return false;
    }

    if (!FilesystemUtils::storeJson(filesystem, fn, txDoc)) {
        AO_DBG_ERR("FS error");
        return false;
    }

    //Data committed to memory; now update meta structures
    
    if (!transaction->isPending()) {
        
        AO_DBG_DEBUG("Drop completed transaction");
        transactions.erase(found);
        transaction = nullptr;
        auto beginNew = txEnd;
        if (!transactions.empty()) {
            beginNew = transactions.front()->getTxNr();
        }
        if (beginNew != (uint) *txBegin) {
            *txBegin = beginNew;
            configuration_save();
        }
    }

    //success
    return true;
}

TransactionStore::TransactionStore(TransactionService& context, uint nConnectors, std::shared_ptr<FilesystemAdapter> filesystem) {
    
    for (uint i = 0; i < nConnectors; i++) {
        connectors.push_back(std::unique_ptr<ConnectorTransactionStore>(
            new ConnectorTransactionStore(context, i, filesystem)));
    }
}

std::shared_ptr<Transaction> TransactionStore::getActiveTransaction(uint connectorId) {
    if (connectorId >= connectors.size()) {
        AO_DBG_ERR("Invalid connectorId");
        return nullptr;
    }
    return connectors[connectorId]->getActiveTransaction();
}

std::shared_ptr<Transaction> TransactionStore::getTransactionSync(uint connectorId) {
    if (connectorId >= connectors.size()) {
        AO_DBG_ERR("Invalid connectorId");
        return nullptr;
    }
    return connectors[connectorId]->getTransactionSync();
}

bool TransactionStore::commit(Transaction *transaction) {
    if (!transaction) {
        AO_DBG_ERR("Invalid arg");
        return false;
    }
    auto connectorId = transaction->getConnectorId();
    if (connectorId < 0 || connectorId >= connectors.size()) {
        AO_DBG_ERR("Invalid tx");
        return false;
    }
    return connectors[connectorId]->commit(transaction);
}

void TransactionStore::restorePendingTransactions() {
    for (auto& connector : connectors) {
        connector->restorePendingTransactions();
    }
}

void TransactionStore::submitPendingOperations() {
    for (auto& connector : connectors) {
        connector->submitPendingOperations();
    }
}
