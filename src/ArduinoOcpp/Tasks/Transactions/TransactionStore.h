// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef TRANSACTIONSTORE_H
#define TRANSACTIONSTORE_H

#include <ArduinoOcpp/Tasks/Transactions/Transaction.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Core/FilesystemAdapter.h>
#include <ArduinoOcpp/Tasks/Transactions/TransactionSequence.h>
#include <deque>

namespace ArduinoOcpp {

class TransactionService;
class ConnectorTransactionSequence;

class ConnectorTransactionStore {
private:
    TransactionService& context;
    const uint connectorId;

    std::shared_ptr<FilesystemAdapter> filesystem;
    std::shared_ptr<Configuration<int>> txBegin; //The Tx file names are consecutively numbered; first number
    uint txEnd = 0; //one place after last number
    
    std::deque<std::shared_ptr<Transaction>> transactions;

    std::shared_ptr<Transaction> makeTransaction();

public:
    ConnectorTransactionStore(TransactionService& context, uint nConnectors, std::shared_ptr<FilesystemAdapter> filesystem);
    
    std::shared_ptr<Transaction> getActiveTransaction();
    std::shared_ptr<Transaction> getTransactionSync();
    bool commit(Transaction *transaction);

    void restorePendingTransactions();
    void submitPendingOperations();
};

class TransactionStore {
private:
    std::vector<std::unique_ptr<ConnectorTransactionStore>> connectors;

public:
    TransactionStore(TransactionService& context, uint nConnectors, std::shared_ptr<FilesystemAdapter> filesystem);

    //std::shared_ptr<Transaction> makeTransaction(uint connectorId);
    std::shared_ptr<Transaction> getActiveTransaction(uint connectorId);
    std::shared_ptr<Transaction> getTransactionSync(uint connectorId); //fron element of the tx queue; tx which is being executed at the server now
    bool commit(Transaction *transaction);

    void restorePendingTransactions();
    void submitPendingOperations();
};

}

#endif
