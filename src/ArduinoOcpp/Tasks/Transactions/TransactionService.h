// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef TRANSACTIONSERVICE_H
#define TRANSACTIONSERVICE_H

#include <ArduinoOcpp/Tasks/Transactions/TransactionStore.h>
#include <ArduinoOcpp/Tasks/Transactions/TransactionSequence.h>
#include <ArduinoOcpp/Tasks/Transactions/OrderedOperationsQueue.h>
#include <memory>

namespace ArduinoOcpp {

class OcppEngine;
class FilesystemAdapter;
class OcppOperation;

class TransactionService {
private:
    OcppEngine& context;

    TransactionSequence txSequence;
    TransactionStore txStore;
    OrderedOperationsQueue restoredOps;//restore pending operations from flash in case of a power loss
    bool restoredOpsInitiated = false;

    std::shared_ptr<Configuration<int>> lastInitiatedSeqNr;
    std::shared_ptr<Configuration<int>> lastInitiatedMsgCnt; //use this msgId for the current initiated operation
public:
    TransactionService(OcppEngine& context, uint nConnectors, std::shared_ptr<FilesystemAdapter> filesystem);

    void addRestoredOperation(uint seqNr, std::unique_ptr<OcppOperation> op);
    void initiateRestoredOperations();

    void setInitiatedMsgId(uint seqNr, int msgId);

    TransactionStore& getTransactionStore() {return txStore;}
    TransactionSequence& getTransactionSequence() {return txSequence;}
};

}

#endif
