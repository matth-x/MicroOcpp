// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Tasks/Transactions/TransactionService.h>
#include <ArduinoOcpp/Tasks/Transactions/TransactionStore.h>
#include <ArduinoOcpp/Tasks/Transactions/OrderedOperationsQueue.h>
#include <ArduinoOcpp/Tasks/Transactions/TransactionSequence.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Core/OcppOperation.h>
#include <ArduinoOcpp/Debug.h>

using namespace ArduinoOcpp;

#define AO_TXSERV_FN AO_FILENAME_PREFIX "/txserv.cnf"

TransactionService::TransactionService(OcppEngine& context, uint nConnectors, std::shared_ptr<FilesystemAdapter> filesystem) : 
        context(context),
        txStore(*this, nConnectors, filesystem) {
    
    lastInitiatedSeqNr = declareConfiguration<int>("LastSeqNr", MAX_TXEVENT_CNT, AO_TXSERV_FN, false, false, true, false);
    lastInitiatedMsgCnt = declareConfiguration<int>("LastMsgId", 1100000, AO_TXSERV_FN, false, false, true, false);

    txStore.restorePendingTransactions();
    txStore.submitPendingOperations();
}

void TransactionService::addRestoredOperation(uint seqNr, std::unique_ptr<OcppOperation> o) {
    if (restoredOpsInitiated) {
        AO_DBG_ERR("Can only restore at initialization. Abort");
        return;
    }
    if (seqNr == (uint) *lastInitiatedSeqNr) {
        AO_DBG_DEBUG("Rebase msgIds: %i", (int) *lastInitiatedMsgCnt);
        o->rebaseMsgId(*lastInitiatedMsgCnt);
    }
    restoredOps.addOcppOperation(seqNr, std::move(o));
}

void TransactionService::initiateRestoredOperations() {
    if (restoredOpsInitiated) {
        AO_DBG_ERR("Called two times. Abort");
        return;
    }

    restoredOps.sort(txSequence.getSeqEnd());

    std::vector<std::unique_ptr<OcppOperation>> res;

    restoredOps.moveTo(res);

    for (auto operation = res.begin(); operation != res.end(); operation++) {
        context.initiateOperation(std::move(*operation));
    }
    res.clear();

    restoredOpsInitiated = true;
}

void TransactionService::setInitiatedMsgId(uint seqNr, int msgId) {
    if (seqNr >= MAX_TXEVENT_CNT) {
        AO_DBG_ERR("Invalid params");
        return;
    }
    if (*lastInitiatedSeqNr == seqNr && *lastInitiatedMsgCnt == msgId) {
        //Nothing to update
        return;
    }

    AO_DBG_DEBUG("Set tx-related msg Id");
    *lastInitiatedSeqNr = seqNr;
    *lastInitiatedMsgCnt = msgId;
    configuration_save();
}
