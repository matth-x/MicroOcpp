// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Model/Transactions/TransactionDeserialize.h>
#include <MicroOcpp/Operations/StartTransaction.h>
#include <MicroOcpp/Operations/StopTransaction.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Debug.h>

#include <algorithm>

using namespace MicroOcpp;

#define MO_TXSTORE_META_FN MO_FILENAME_PREFIX "txstore.jsn"

ConnectorTransactionStore::ConnectorTransactionStore(TransactionStore& context, unsigned int connectorId, std::shared_ptr<FilesystemAdapter> filesystem) :
        context(context),
        connectorId(connectorId),
        filesystem(filesystem) {

    snprintf(txBeginKey, sizeof(txBeginKey), MO_TXSTORE_TXBEGIN_KEY "%u", connectorId);
    txBeginInt = declareConfiguration<int>(txBeginKey, 0, MO_TXSTORE_META_FN, false, false, false);

    snprintf(txEndKey, sizeof(txEndKey), MO_TXSTORE_TXEND_KEY "%u", connectorId);
    txEndInt = declareConfiguration<int>(txEndKey, 0, MO_TXSTORE_META_FN, false, false, false);
}

ConnectorTransactionStore::~ConnectorTransactionStore() {
    if (txBeginInt->getKey() == txBeginKey) {
        txBeginInt->setKey(nullptr);
    }
    if (txEndInt->getKey() == txEndKey) {
        txEndInt->setKey(nullptr);
    }
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
        MO_DBG_DEBUG("no FS adapter");
        return nullptr;
    }

    char fn [MO_MAX_PATH_SIZE] = {'\0'};
    auto ret = snprintf(fn, MO_MAX_PATH_SIZE, MO_FILENAME_PREFIX "tx" "-%u-%u.jsn", connectorId, txNr);
    if (ret < 0 || ret >= MO_MAX_PATH_SIZE) {
        MO_DBG_ERR("fn error: %i", ret);
        return nullptr;
    }

    size_t msize;
    if (filesystem->stat(fn, &msize) != 0) {
        MO_DBG_DEBUG("%u-%u does not exist", connectorId, txNr);
        return nullptr;
    }

    auto doc = FilesystemUtils::loadJson(filesystem, fn);

    if (!doc) {
        MO_DBG_ERR("memory corruption");
        return nullptr;
    }

    auto transaction = std::make_shared<Transaction>(*this, connectorId, txNr);
    JsonObject txJson = doc->as<JsonObject>();
    if (!deserializeTransaction(*transaction, txJson)) {
        MO_DBG_ERR("deserialization error");
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
    
    if (!txBeginInt || txBeginInt->getInt() < 0 || !txEndInt || txEndInt->getInt() < 0) {
        MO_DBG_ERR("memory corruption");
        return nullptr;
    }

    //check if maximum number of queued tx already reached
    if ((txEndInt->getInt() + MAX_TX_CNT - txBeginInt->getInt()) % MAX_TX_CNT >= MO_TXRECORD_SIZE) {
        //limit reached

        if (!silent) {
            //normal tx -> abort
            return nullptr;
        }
        //special case: silent tx -> create tx anyway, but should be deleted immediately after charging session
    }

    auto transaction = std::make_shared<Transaction>(*this, connectorId, (unsigned int) txEndInt->getInt(), silent);

    txEndInt->setInt((txEndInt->getInt() + 1) % MAX_TX_CNT);
    configuration_save();

    if (!commit(transaction.get())) {
        MO_DBG_ERR("FS error");
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
    if (!txEndInt || txEndInt->getInt() < 0) {
        MO_DBG_ERR("memory corruption");
        return nullptr;
    }

    unsigned int latest = ((unsigned int) txEndInt->getInt() + MAX_TX_CNT - 1) % MAX_TX_CNT;

    return getTransaction(latest);
}

bool ConnectorTransactionStore::commit(Transaction *transaction) {

    if (!filesystem) {
        MO_DBG_DEBUG("no FS: nothing to commit");
        return true;
    }

    char fn [MO_MAX_PATH_SIZE] = {'\0'};
    auto ret = snprintf(fn, MO_MAX_PATH_SIZE, MO_FILENAME_PREFIX "tx" "-%u-%u.jsn", connectorId, transaction->getTxNr());
    if (ret < 0 || ret >= MO_MAX_PATH_SIZE) {
        MO_DBG_ERR("fn error: %i", ret);
        return false;
    }
    
    DynamicJsonDocument txDoc {0};
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

bool ConnectorTransactionStore::remove(unsigned int txNr) {

    if (!filesystem) {
        MO_DBG_DEBUG("no FS: nothing to remove");
        return true;
    }

    char fn [MO_MAX_PATH_SIZE] = {'\0'};
    auto ret = snprintf(fn, MO_MAX_PATH_SIZE, MO_FILENAME_PREFIX "tx" "-%u-%u.jsn", connectorId, txNr);
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

int ConnectorTransactionStore::getTxBegin() {
    if (!txBeginInt || txBeginInt->getInt() < 0) {
        MO_DBG_ERR("memory corruption");
        return -1;
    }

    return txBeginInt->getInt();
}

int ConnectorTransactionStore::getTxEnd() {
    if (!txEndInt || txEndInt->getInt() < 0) {
        MO_DBG_ERR("memory corruption");
        return -1;
    }

    return txEndInt->getInt();
}

void ConnectorTransactionStore::setTxBegin(unsigned int txNr) {
    if (!txBeginInt || txBeginInt->getInt() < 0) {
        MO_DBG_ERR("memory corruption");
        return;
    }

    txBeginInt->setInt(txNr);
    configuration_save();
}

void ConnectorTransactionStore::setTxEnd(unsigned int txNr) {
    if (!txBeginInt || txBeginInt->getInt() < 0 || !txEndInt || txEndInt->getInt() < 0) {
        MO_DBG_ERR("memory corruption");
        return;
    }

    txEndInt->setInt(txNr);
    configuration_save();
}

unsigned int ConnectorTransactionStore::size() {
    if (!txBeginInt || txBeginInt->getInt() < 0 || !txEndInt || txEndInt->getInt() < 0) {
        MO_DBG_ERR("memory corruption");
        return 0;
    }

    return (txEndInt->getInt() + MAX_TX_CNT - txBeginInt->getInt()) % MAX_TX_CNT;
}

TransactionStore::TransactionStore(unsigned int nConnectors, std::shared_ptr<FilesystemAdapter> filesystem) {
    
    for (unsigned int i = 0; i < nConnectors; i++) {
        connectors.push_back(std::unique_ptr<ConnectorTransactionStore>(
            new ConnectorTransactionStore(*this, i, filesystem)));
    }

    configuration_load(MO_TXSTORE_META_FN);
}

std::shared_ptr<Transaction> TransactionStore::getLatestTransaction(unsigned int connectorId) {
    if (connectorId >= connectors.size()) {
        MO_DBG_ERR("Invalid connectorId");
        return nullptr;
    }
    return connectors[connectorId]->getLatestTransaction();
}

bool TransactionStore::commit(Transaction *transaction) {
    if (!transaction) {
        MO_DBG_ERR("Invalid arg");
        return false;
    }
    auto connectorId = transaction->getConnectorId();
    if (connectorId >= connectors.size()) {
        MO_DBG_ERR("Invalid tx");
        return false;
    }
    return connectors[connectorId]->commit(transaction);
}

std::shared_ptr<Transaction> TransactionStore::getTransaction(unsigned int connectorId, unsigned int txNr) {
    if (connectorId >= connectors.size()) {
        MO_DBG_ERR("Invalid connectorId");
        return nullptr;
    }
    return connectors[connectorId]->getTransaction(txNr);
}

std::shared_ptr<Transaction> TransactionStore::createTransaction(unsigned int connectorId, bool silent) {
    if (connectorId >= connectors.size()) {
        MO_DBG_ERR("Invalid connectorId");
        return nullptr;
    }
    return connectors[connectorId]->createTransaction(silent);
}

bool TransactionStore::remove(unsigned int connectorId, unsigned int txNr) {
    if (connectorId >= connectors.size()) {
        MO_DBG_ERR("Invalid connectorId");
        return false;
    }
    return connectors[connectorId]->remove(txNr);
}

int TransactionStore::getTxBegin(unsigned int connectorId) {
    if (connectorId >= connectors.size()) {
        MO_DBG_ERR("Invalid connectorId");
        return -1;
    }
    return connectors[connectorId]->getTxBegin();
}

int TransactionStore::getTxEnd(unsigned int connectorId) {
    if (connectorId >= connectors.size()) {
        MO_DBG_ERR("Invalid connectorId");
        return -1;
    }
    return connectors[connectorId]->getTxEnd();
}

void TransactionStore::setTxBegin(unsigned int connectorId, unsigned int txNr) {
    if (connectorId >= connectors.size()) {
        MO_DBG_ERR("Invalid connectorId");
        return;
    }
    return connectors[connectorId]->setTxBegin(txNr);
}

void TransactionStore::setTxEnd(unsigned int connectorId, unsigned int txNr) {
    if (connectorId >= connectors.size()) {
        MO_DBG_ERR("Invalid connectorId");
        return;
    }
    return connectors[connectorId]->setTxEnd(txNr);
}

unsigned int TransactionStore::size(unsigned int connectorId) {
    if (connectorId >= connectors.size()) {
        MO_DBG_ERR("Invalid connectorId");
        return 0;
    }
    return connectors[connectorId]->size();
}
