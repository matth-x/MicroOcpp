// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef TRANSACTIONSTORE_H
#define TRANSACTIONSTORE_H

#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <deque>

#define MAX_TX_CNT 100000U

#ifndef MO_TXRECORD_SIZE
#define MO_TXRECORD_SIZE 4 //no. of tx to hold on flash storage
#endif

#define MO_TXSTORE_TXBEGIN_KEY "txBegin_"
#define MO_TXSTORE_TXEND_KEY "txEnd_"

namespace MicroOcpp {

class TransactionStore;

class ConnectorTransactionStore {
private:
    TransactionStore& context;
    const unsigned int connectorId;

    std::shared_ptr<FilesystemAdapter> filesystem;
    std::shared_ptr<Configuration> txBeginInt; //if txNr < txBegin, tx has been safely deleted
    char txBeginKey [sizeof(MO_TXSTORE_TXBEGIN_KEY "xxx") + 1]; //"xxx": placeholder for connectorId

    std::shared_ptr<Configuration> txEndInt;
    char txEndKey [sizeof(MO_TXSTORE_TXEND_KEY "xxx") + 1];
    
    std::deque<std::weak_ptr<Transaction>> transactions;

public:
    ConnectorTransactionStore(TransactionStore& context, unsigned int connectorId, std::shared_ptr<FilesystemAdapter> filesystem);
    ConnectorTransactionStore(const ConnectorTransactionStore&) = delete;
    ConnectorTransactionStore(ConnectorTransactionStore&&) = delete;
    ConnectorTransactionStore& operator=(const ConnectorTransactionStore&) = delete;

    ~ConnectorTransactionStore();
    
    std::shared_ptr<Transaction> getLatestTransaction();
    bool commit(Transaction *transaction);

    std::shared_ptr<Transaction> getTransaction(unsigned int txNr);
    std::shared_ptr<Transaction> createTransaction(bool silent = false);

    bool remove(unsigned int txNr);

    int getTxBegin();
    int getTxEnd();
    void setTxBegin(unsigned int txNr);
    void setTxEnd(unsigned int txNr);

    unsigned int size();
};

class TransactionStore {
private:
    std::vector<std::unique_ptr<ConnectorTransactionStore>> connectors;
public:
    TransactionStore(unsigned int nConnectors, std::shared_ptr<FilesystemAdapter> filesystem);

    std::shared_ptr<Transaction> getLatestTransaction(unsigned int connectorId);
    bool commit(Transaction *transaction);

    std::shared_ptr<Transaction> getTransaction(unsigned int connectorId, unsigned int txNr);
    std::shared_ptr<Transaction> createTransaction(unsigned int connectorId, bool silent = false);

    bool remove(unsigned int connectorId, unsigned int txNr);

    int getTxBegin(unsigned int connectorId);
    int getTxEnd(unsigned int connectorId);
    void setTxBegin(unsigned int connectorId, unsigned int txNr);
    void setTxEnd(unsigned int connectorId, unsigned int txNr);

    unsigned int size(unsigned int connectorId);
};

}

#endif
