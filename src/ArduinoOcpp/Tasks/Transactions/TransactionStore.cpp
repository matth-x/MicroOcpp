// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
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

#define AO_TXSTORE_META_FN AO_FILENAME_PREFIX "txstore.jsn"

ConnectorTransactionStore::ConnectorTransactionStore(TransactionStore& context, unsigned int connectorId, std::shared_ptr<FilesystemAdapter> filesystem) :
        context(context),
        connectorId(connectorId),
        filesystem(filesystem) {

    char key [30] = {'\0'};
    if (snprintf(key, 30, "AO_txEnd_%u", connectorId) < 0) {
        AO_DBG_ERR("Invalid key");
        (void)0;
    }
    txEnd = declareConfiguration<int>(key, 0, AO_TXSTORE_META_FN, false, false, true, false);

    if (snprintf(key, 30, "AO_txBegin_%u", connectorId) < 0) {
        AO_DBG_ERR("Invalid key");
        (void)0;
    }
    txBegin = declareConfiguration<int>(key, 0, AO_TXSTORE_META_FN, false, false, true, false);
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

    char fn [AO_MAX_PATH_SIZE] = {'\0'};
    auto ret = snprintf(fn, AO_MAX_PATH_SIZE, AO_FILENAME_PREFIX "tx" "-%u-%u.jsn", connectorId, txNr);
    if (ret < 0 || ret >= AO_MAX_PATH_SIZE) {
        AO_DBG_ERR("fn error: %i", ret);
        return nullptr;
    }

    size_t msize;
    if (filesystem->stat(fn, &msize) != 0) {
        AO_DBG_DEBUG("%u-%u does not exist", connectorId, txNr);
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
    transactions.erase(std::remove_if(transactions.begin(), transactions.end(),
            [](std::weak_ptr<Transaction> tx) {
                return tx.expired();
            }),
            transactions.end());

    transactions.push_back(transaction);
    return transaction;
}

std::shared_ptr<Transaction> ConnectorTransactionStore::createTransaction(bool silent) {
    
    if (!txBegin || *txBegin < 0 || !txEnd || *txEnd < 0) {
        AO_DBG_ERR("memory corruption");
        return nullptr;
    }

    //check if maximum number of queued tx already reached
    if ((*txEnd + MAX_TX_CNT - *txBegin) % MAX_TX_CNT >= AO_TXRECORD_SIZE) {
        //limit reached

        if (!silent) {
            //normal tx -> abort
            return nullptr;
        }
        //special case: silent tx -> create tx anyway, but should be deleted immediately after charging session
    }

    auto transaction = std::make_shared<Transaction>(*this, connectorId, (unsigned int) *txEnd, silent);

    *txEnd = (*txEnd + 1) % MAX_TX_CNT;
    configuration_save();

    if (!commit(transaction.get())) {
        AO_DBG_ERR("FS error");
        return nullptr;
    }

    //before adding new entry, clean cache
    transactions.erase(std::remove_if(transactions.begin(), transactions.end(),
            [](std::weak_ptr<Transaction> tx) {
                return tx.expired();
            }),
            transactions.end());

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

    char fn [AO_MAX_PATH_SIZE] = {'\0'};
    auto ret = snprintf(fn, AO_MAX_PATH_SIZE, AO_FILENAME_PREFIX "tx" "-%u-%u.jsn", connectorId, transaction->getTxNr());
    if (ret < 0 || ret >= AO_MAX_PATH_SIZE) {
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

bool ConnectorTransactionStore::remove(unsigned int txNr) {

    if (!filesystem) {
        AO_DBG_DEBUG("no FS: nothing to remove");
        return true;
    }

    char fn [AO_MAX_PATH_SIZE] = {'\0'};
    auto ret = snprintf(fn, AO_MAX_PATH_SIZE, AO_FILENAME_PREFIX "tx" "-%u-%u.jsn", connectorId, txNr);
    if (ret < 0 || ret >= AO_MAX_PATH_SIZE) {
        AO_DBG_ERR("fn error: %i", ret);
        return false;
    }

    size_t msize;
    if (filesystem->stat(fn, &msize) != 0) {
        AO_DBG_DEBUG("%s already removed", fn);
        return true;
    }

    AO_DBG_DEBUG("remove %s", fn);
    
    return filesystem->remove(fn);
}

int ConnectorTransactionStore::getTxBegin() {
    if (!txBegin || *txBegin < 0) {
        AO_DBG_ERR("memory corruption");
        return -1;
    }

    return *txBegin;
}

int ConnectorTransactionStore::getTxEnd() {
    if (!txBegin || *txBegin < 0) {
        AO_DBG_ERR("memory corruption");
        return -1;
    }

    return *txEnd;
}

void ConnectorTransactionStore::setTxBegin(unsigned int txNr) {
    if (!txBegin || *txBegin < 0) {
        AO_DBG_ERR("memory corruption");
        return;
    }

    *txBegin = txNr;
    configuration_save();
}

void ConnectorTransactionStore::setTxEnd(unsigned int txNr) {
    if (!txBegin || *txBegin < 0 || !txEnd || *txEnd < 0) {
        AO_DBG_ERR("memory corruption");
        return;
    }

    *txEnd = txNr;
    configuration_save();
}

unsigned int ConnectorTransactionStore::size() {
    if (!txBegin || *txBegin < 0 || !txEnd || *txEnd < 0) {
        AO_DBG_ERR("memory corruption");
        return 0;
    }

    return (*txEnd + MAX_TX_CNT - *txBegin) % MAX_TX_CNT;
}

TransactionStore::TransactionStore(unsigned int nConnectors, std::shared_ptr<FilesystemAdapter> filesystem) {
    
    for (unsigned int i = 0; i < nConnectors; i++) {
        connectors.push_back(std::unique_ptr<ConnectorTransactionStore>(
            new ConnectorTransactionStore(*this, i, filesystem)));
    }
}

std::shared_ptr<Transaction> TransactionStore::getLatestTransaction(unsigned int connectorId) {
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
    if (connectorId >= connectors.size()) {
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

std::shared_ptr<Transaction> TransactionStore::createTransaction(unsigned int connectorId, bool silent) {
    if (connectorId >= connectors.size()) {
        AO_DBG_ERR("Invalid connectorId");
        return nullptr;
    }
    return connectors[connectorId]->createTransaction(silent);
}

bool TransactionStore::remove(unsigned int connectorId, unsigned int txNr) {
    if (connectorId >= connectors.size()) {
        AO_DBG_ERR("Invalid connectorId");
        return false;
    }
    return connectors[connectorId]->remove(txNr);
}

int TransactionStore::getTxBegin(unsigned int connectorId) {
    if (connectorId >= connectors.size()) {
        AO_DBG_ERR("Invalid connectorId");
        return -1;
    }
    return connectors[connectorId]->getTxBegin();
}

int TransactionStore::getTxEnd(unsigned int connectorId) {
    if (connectorId >= connectors.size()) {
        AO_DBG_ERR("Invalid connectorId");
        return -1;
    }
    return connectors[connectorId]->getTxEnd();
}

void TransactionStore::setTxBegin(unsigned int connectorId, unsigned int txNr) {
    if (connectorId >= connectors.size()) {
        AO_DBG_ERR("Invalid connectorId");
        return;
    }
    return connectors[connectorId]->setTxBegin(txNr);
}

void TransactionStore::setTxEnd(unsigned int connectorId, unsigned int txNr) {
    if (connectorId >= connectors.size()) {
        AO_DBG_ERR("Invalid connectorId");
        return;
    }
    return connectors[connectorId]->setTxEnd(txNr);
}

unsigned int TransactionStore::size(unsigned int connectorId) {
    if (connectorId >= connectors.size()) {
        AO_DBG_ERR("Invalid connectorId");
        return 0;
    }
    return connectors[connectorId]->size();
}
