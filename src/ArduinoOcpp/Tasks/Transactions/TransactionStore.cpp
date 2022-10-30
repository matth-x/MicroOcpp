// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Tasks/Transactions/TransactionStore.h>
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

#define MAX_TX_CNT 100000U

ConnectorTransactionStore::ConnectorTransactionStore(TransactionStore& context, uint connectorId, std::shared_ptr<FilesystemAdapter> filesystem) :
        context(context),
        connectorId(connectorId),
        filesystem(filesystem) {

    char key [30] = {'\0'};
    if (snprintf(key, 30, "AO_txEnd_%u", connectorId) < 0) {
        AO_DBG_ERR("Invalid key");
        (void)0;
    }
    txEnd = declareConfiguration<int>(key, 0, AO_TXSTORE_META_FN, false, false, true, false);
}

std::shared_ptr<Transaction> ConnectorTransactionStore::getTransaction(unsigned int txNr) {

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
        AO_DBG_DEBUG("no FS adapter");
        return nullptr;
    }

    char fn [MAX_PATH_SIZE] = {'\0'};
    auto ret = snprintf(fn, MAX_PATH_SIZE, AO_TXSTORE_DIR "tx" "-%u-%u.jsn", connectorId, txNr);
    if (ret < 0 || ret >= MAX_PATH_SIZE) {
        AO_DBG_ERR("fn error: %i", ret);
        return nullptr;
    }

    size_t msize;
    if (filesystem->stat(fn, &msize) != 0) {
        AO_DBG_DEBUG("tx does not exist");
        return nullptr;
    }

    auto doc = FilesystemUtils::loadJson(filesystem, fn);

    if (!doc) {
        AO_DBG_ERR("memory corruption");
        return nullptr;
    }

    auto transaction = std::make_shared<Transaction>(*this, connectorId, txNr);
    JsonObject txJson = doc->as<JsonObject>();
    if (!transaction->deserializeSessionState(txJson)) {
        AO_DBG_ERR("deserialization error");
        return nullptr;
    }

    //before adding new entry, clean cache
    std::remove_if(transactions.begin(), transactions.end(), [](std::weak_ptr<Transaction> tx) {
        return tx.expired();
    });

    transactions.push_back(transaction);
    return transaction;
}

std::shared_ptr<Transaction> ConnectorTransactionStore::createTransaction() {

    if (!txEnd || *txEnd < 0) {
        AO_DBG_ERR("memory corruption");
        return nullptr;
    }

    auto transaction = std::make_shared<Transaction>(*this, connectorId, (unsigned int) *txEnd);

    *txEnd = (*txEnd + 1) % MAX_TX_CNT;
    configuration_save();

    if (!commit(transaction.get())) {
        AO_DBG_ERR("FS error");
        return nullptr;
    }

    //before adding new entry, clean cache
    std::remove_if(transactions.begin(), transactions.end(), [](std::weak_ptr<Transaction> tx) {
        return tx.expired();
    });

    transactions.push_back(transaction);
    return transaction;
}

std::shared_ptr<Transaction> ConnectorTransactionStore::getLatestTransaction() {
    if (!txEnd || *txEnd < 0) {
        AO_DBG_ERR("memory corruption");
        return nullptr;
    }

    unsigned int latest = ((unsigned int) *txEnd + MAX_TX_CNT - 1) % MAX_TX_CNT;

    return getTransaction(latest);
}

bool ConnectorTransactionStore::commit(Transaction *transaction) {

    if (!filesystem) {
        AO_DBG_DEBUG("no FS: nothing to commit");
        return true;
    }

    char fn [MAX_PATH_SIZE] = {'\0'};
    auto ret = snprintf(fn, MAX_PATH_SIZE, AO_TXSTORE_DIR "tx" "-%u-%u.jsn", connectorId, transaction->getTxNr());
    if (ret < 0 || ret >= MAX_PATH_SIZE) {
        AO_DBG_ERR("fn error: %i", ret);
        return false;
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

    //success
    return true;
}

TransactionStore::TransactionStore(uint nConnectors, std::shared_ptr<FilesystemAdapter> filesystem) {
    
    for (uint i = 0; i < nConnectors; i++) {
        connectors.push_back(std::unique_ptr<ConnectorTransactionStore>(
            new ConnectorTransactionStore(*this, i, filesystem)));
    }
}

std::shared_ptr<Transaction> TransactionStore::getLatestTransaction(uint connectorId) {
    if (connectorId >= connectors.size()) {
        AO_DBG_ERR("Invalid connectorId");
        return nullptr;
    }
    return connectors[connectorId]->getLatestTransaction();
}

bool TransactionStore::commit(Transaction *transaction) {
    if (!transaction) {
        AO_DBG_ERR("Invalid arg");
        return false;
    }
    auto connectorId = transaction->getConnectorId();
    if (connectorId < 0 || (size_t) connectorId >= connectors.size()) {
        AO_DBG_ERR("Invalid tx");
        return false;
    }
    return connectors[connectorId]->commit(transaction);
}

std::shared_ptr<Transaction> TransactionStore::getTransaction(unsigned int connectorId, unsigned int txNr) {
    if (connectorId >= connectors.size()) {
        AO_DBG_ERR("Invalid connectorId");
        return nullptr;
    }
    return connectors[connectorId]->getTransaction(txNr);
}

std::shared_ptr<Transaction> TransactionStore::createTransaction(unsigned int connectorId) {
    if (connectorId >= connectors.size()) {
        AO_DBG_ERR("Invalid connectorId");
        return nullptr;
    }
    return connectors[connectorId]->createTransaction();
}
