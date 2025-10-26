// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_TRANSACTIONSTORE_H
#define MO_TRANSACTIONSTORE_H

#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Model/Common/EvseId.h>
#include <MicroOcpp/Version.h>

#if MO_OCPP_V16

#ifndef MO_STOPTXDATA_MAX_SIZE
#define MO_STOPTXDATA_MAX_SIZE 4
#endif

#ifndef MO_STOPTXDATA_DIGITS
#define MO_STOPTXDATA_DIGITS 1 //digits needed to print MO_STOPTXDATA_MAX_SIZE-1 (="3", i.e. 1 digit)
#endif

namespace MicroOcpp {

class Context;

namespace v16 {

class Transaction;

namespace TransactionStore {

bool printTxFname(char *fname, size_t size, unsigned int evseId, unsigned int txNr);

FilesystemUtils::LoadStatus load(MO_FilesystemAdapter *filesystem, Context& context, unsigned int evseId, unsigned int txNr, Transaction& transaction);
FilesystemUtils::StoreStatus store(MO_FilesystemAdapter *filesystem, Context& context, Transaction& transaction);
bool remove(MO_FilesystemAdapter *filesystem, unsigned int evseId, unsigned int txNr);

} //namespace TransactionStore
} //namespace v16
} //namespace MicroOcpp
#endif //MO_OCPP_V16

#if MO_ENABLE_V201

#ifndef MO_TXEVENTRECORD_SIZE
#define MO_TXEVENTRECORD_SIZE 10 //maximum number of of txEvents per tx to hold on flash storage
#endif

#ifndef MO_TXEVENTRECORD_DIGITS
#define MO_TXEVENTRECORD_DIGITS 1 //digits needed to print MO_TXEVENTRECORD_SIZE-1 (="9", i.e. 1 digit)
#endif

namespace MicroOcpp {

class Context;

namespace v201 {

class Transaction;
class TransactionEventData;
class TransactionStore;

class TransactionStoreEvse : public MemoryManaged {
private:
    Context& context;
    const unsigned int evseId;

    MO_FilesystemAdapter *filesystem = nullptr;

    bool serializeTransaction(Transaction& tx, JsonObject out);
    bool serializeTransactionEvent(TransactionEventData& txEvent, JsonObject out);
    bool deserializeTransaction(Transaction& tx, JsonObject in);
    bool deserializeTransactionEvent(TransactionEventData& txEvent, JsonObject in);

    bool commit(Transaction& transaction, TransactionEventData *transactionEvent);

    bool printTxEventFname(char *fname, size_t size, unsigned int evseId, unsigned int txNr, unsigned int seqNo);

public:
    TransactionStoreEvse(Context& context, unsigned int evseId);

    bool setup();

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
    Context& context;
    TransactionStoreEvse* evses [MO_NUM_EVSEID] = {nullptr};
    unsigned int numEvseId = MO_NUM_EVSEID;
public:
    TransactionStore(Context& context);

    ~TransactionStore();

    bool setup();

    TransactionStoreEvse *getEvse(unsigned int evseId);
};

} //namespace v201
} //namespace MicroOcpp
#endif //MO_ENABLE_V201

#endif
