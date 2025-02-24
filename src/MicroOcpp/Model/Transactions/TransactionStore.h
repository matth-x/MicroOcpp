// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_TRANSACTIONSTORE_H
#define MO_TRANSACTIONSTORE_H

#include <MicroOcpp/Version.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Memory.h>

namespace MicroOcpp {

class TransactionStore;

class TransactionStoreEvse : public MemoryManaged {
private:
    TransactionStore& context;
    const unsigned int connectorId;

    std::shared_ptr<FilesystemAdapter> filesystem;
    
    Vector<std::weak_ptr<Transaction>> transactions;

public:
    TransactionStoreEvse(TransactionStore& context, unsigned int connectorId, std::shared_ptr<FilesystemAdapter> filesystem);
    TransactionStoreEvse(const TransactionStoreEvse&) = delete;
    TransactionStoreEvse(TransactionStoreEvse&&) = delete;
    TransactionStoreEvse& operator=(const TransactionStoreEvse&) = delete;

    ~TransactionStoreEvse();

    bool commit(Transaction *transaction);

    std::shared_ptr<Transaction> getTransaction(unsigned int txNr);
    std::shared_ptr<Transaction> createTransaction(unsigned int txNr, bool silent = false);

    bool remove(unsigned int txNr);
};

}

#if MO_ENABLE_V201

#ifndef MO_TXEVENTRECORD_SIZE_V201
#define MO_TXEVENTRECORD_SIZE_V201 10 //maximum number of of txEvents per tx to hold on flash storage
#endif

namespace MicroOcpp {
namespace Ocpp201 {

class TransactionStore;

class TransactionStoreEvse : public MemoryManaged {
private:
    TransactionStore& txStore;
    const unsigned int evseId;

    std::shared_ptr<FilesystemAdapter> filesystem;

    bool serializeTransaction(Transaction& tx, JsonObject out);
    bool serializeTransactionEvent(TransactionEventData& txEvent, JsonObject out);
    bool deserializeTransaction(Transaction& tx, JsonObject in);
    bool deserializeTransactionEvent(TransactionEventData& txEvent, JsonObject in);

    bool commit(Transaction& transaction, TransactionEventData *transactionEvent);

public:
    TransactionStoreEvse(TransactionStore& txStore, unsigned int evseId, std::shared_ptr<FilesystemAdapter> filesystem);

    bool discoverStoredTx(unsigned int& txNrBeginOut, unsigned int& txNrEndOut);

    bool commit(Transaction *transaction);
    bool commit(TransactionEventData *transactionEvent);

    std::unique_ptr<Transaction> loadTransaction(unsigned int txNr);
    std::unique_ptr<Transaction> createTransaction(unsigned int txNr, const char *txId);

    std::unique_ptr<TransactionEventData> createTransactionEvent(Transaction& tx);
    std::unique_ptr<TransactionEventData> loadTransactionEvent(Transaction& tx, unsigned int seqNo);

    bool remove(unsigned int txNr);
    bool remove(Transaction& tx, unsigned int seqNo);
};

class TransactionStore : public MemoryManaged {
private:
    TransactionStoreEvse *evses [MO_NUM_EVSEID] = {nullptr};
public:
    TransactionStore(std::shared_ptr<FilesystemAdapter> filesystem, size_t numEvses);

    ~TransactionStore();

    TransactionStoreEvse *getEvse(unsigned int evseId);
};

} //namespace Ocpp201
} //namespace MicroOcpp

#endif //MO_ENABLE_V201

#endif
