// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef TRANSACTIONSTORE_H
#define TRANSACTIONSTORE_H

#include <ArduinoOcpp/Tasks/Transactions/Transaction.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Core/FilesystemAdapter.h>
#include <deque>

#define MAX_TX_CNT 100000U

#ifndef AO_TXRECORD_SIZE
#define AO_TXRECORD_SIZE 4 //no. of tx to hold on flash storage
#endif

namespace ArduinoOcpp {

class TransactionStore;

class ConnectorTransactionStore {
private:
    TransactionStore& context;
    const unsigned int connectorId;

    std::shared_ptr<FilesystemAdapter> filesystem;
    std::shared_ptr<Configuration<int>> txBegin; //if txNr < txBegin, tx has been safely deleted
    std::shared_ptr<Configuration<int>> txEnd;
    
    std::deque<std::weak_ptr<Transaction>> transactions;

public:
    ConnectorTransactionStore(TransactionStore& context, unsigned int connectorId, std::shared_ptr<FilesystemAdapter> filesystem);
    
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
