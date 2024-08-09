// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Model/Transactions/TransactionDeserialize.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

ConnectorTransactionStore::ConnectorTransactionStore(TransactionStore& context, unsigned int connectorId, std::shared_ptr<FilesystemAdapter> filesystem) :
        AllocOverrider("v16.Transactions.TransactionStore"),
        context(context),
        connectorId(connectorId),
        filesystem(filesystem),
        transactions{makeMemVector<std::weak_ptr<Transaction>>(getMemoryTag())} {

}

ConnectorTransactionStore::~ConnectorTransactionStore() {

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

std::shared_ptr<Transaction> ConnectorTransactionStore::createTransaction(unsigned int txNr, bool silent) {

    auto transaction = std::make_shared<Transaction>(*this, connectorId, txNr, silent);

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
    
    auto txDoc = initMemJsonDoc(0, getMemoryTag());
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

TransactionStore::TransactionStore(unsigned int nConnectors, std::shared_ptr<FilesystemAdapter> filesystem) :
        AllocOverrider{"v16.Transactions.TransactionStore"}, connectors{makeMemVector<std::unique_ptr<ConnectorTransactionStore>>(getMemoryTag())} {

    for (unsigned int i = 0; i < nConnectors; i++) {
        connectors.push_back(std::unique_ptr<ConnectorTransactionStore>(
            new ConnectorTransactionStore(*this, i, filesystem)));
    }
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

std::shared_ptr<Transaction> TransactionStore::createTransaction(unsigned int connectorId, unsigned int txNr, bool silent) {
    if (connectorId >= connectors.size()) {
        MO_DBG_ERR("Invalid connectorId");
        return nullptr;
    }
    return connectors[connectorId]->createTransaction(txNr, silent);
}

bool TransactionStore::remove(unsigned int connectorId, unsigned int txNr) {
    if (connectorId >= connectors.size()) {
        MO_DBG_ERR("Invalid connectorId");
        return false;
    }
    return connectors[connectorId]->remove(txNr);
}
