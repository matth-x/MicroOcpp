// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef TRANSACTIONSTORE_H
#define TRANSACTIONSTORE_H

#include <ArduinoOcpp/Tasks/Transactions/Transaction.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Core/FilesystemAdapter.h>
#include <deque>

namespace ArduinoOcpp {

class TransactionService;
class ConnectorTransactionSequence;

class ConnectorTransactionStore {
private:
    TransactionService& context;
    const uint connectorId;

    std::shared_ptr<FilesystemAdapter> filesystem;
    std::shared_ptr<Configuration<int>> txEnd;
    
    std::deque<std::weak_ptr<Transaction>> transactions;

public:
    ConnectorTransactionStore(TransactionService& context, uint connectorId, std::shared_ptr<FilesystemAdapter> filesystem);
    
    std::shared_ptr<Transaction> getActiveTransaction();
    std::shared_ptr<Transaction> getTransactionSync();
    bool commit(Transaction *transaction);

    std::shared_ptr<Transaction> getTransaction(unsigned int txNr);
    std::shared_ptr<Transaction> createTransaction();
};

class TransactionStore {
private:
    std::vector<std::unique_ptr<ConnectorTransactionStore>> connectors;
public:
    TransactionStore(TransactionService& context, uint nConnectors, std::shared_ptr<FilesystemAdapter> filesystem);

    std::shared_ptr<Transaction> getActiveTransaction(uint connectorId);
    std::shared_ptr<Transaction> getTransactionSync(uint connectorId); //fron element of the tx queue; tx which is being executed at the server now
    bool commit(Transaction *transaction);

    std::shared_ptr<Transaction> getTransaction(unsigned int connectorId, unsigned int txNr);
    std::shared_ptr<Transaction> createTransaction(unsigned int connectorId);
};

}

#endif
