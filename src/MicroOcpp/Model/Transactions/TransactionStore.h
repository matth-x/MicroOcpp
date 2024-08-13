// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_TRANSACTIONSTORE_H
#define MO_TRANSACTIONSTORE_H

#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Memory.h>

namespace MicroOcpp {

class TransactionStore;

class ConnectorTransactionStore : public MemoryManaged {
private:
    TransactionStore& context;
    const unsigned int connectorId;

    std::shared_ptr<FilesystemAdapter> filesystem;
    
    Vector<std::weak_ptr<Transaction>> transactions;

public:
    ConnectorTransactionStore(TransactionStore& context, unsigned int connectorId, std::shared_ptr<FilesystemAdapter> filesystem);
    ConnectorTransactionStore(const ConnectorTransactionStore&) = delete;
    ConnectorTransactionStore(ConnectorTransactionStore&&) = delete;
    ConnectorTransactionStore& operator=(const ConnectorTransactionStore&) = delete;

    ~ConnectorTransactionStore();

    bool commit(Transaction *transaction);

    std::shared_ptr<Transaction> getTransaction(unsigned int txNr);
    std::shared_ptr<Transaction> createTransaction(unsigned int txNr, bool silent = false);

    bool remove(unsigned int txNr);
};

class TransactionStore : public MemoryManaged {
private:
    Vector<std::unique_ptr<ConnectorTransactionStore>> connectors;
public:
    TransactionStore(unsigned int nConnectors, std::shared_ptr<FilesystemAdapter> filesystem);

    bool commit(Transaction *transaction);

    std::shared_ptr<Transaction> getTransaction(unsigned int connectorId, unsigned int txNr);
    std::shared_ptr<Transaction> createTransaction(unsigned int connectorId, unsigned int txNr, bool silent = false);

    bool remove(unsigned int connectorId, unsigned int txNr);
};

}

#endif
