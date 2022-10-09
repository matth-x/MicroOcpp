// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Tasks/Transactions/TransactionProcess.h>

#include <ArduinoOcpp/Debug.h>

#define AO_TXPROC_FN AO_FILENAME_PREFIX "/txproc.cnf"

using namespace ArduinoOcpp;

TransactionProcess::TransactionProcess(uint connectorId) {
    char key [30] = {'\0'};
    if (snprintf(key, 30, "AO_txNrRef_%u", connectorId) < 0) {
        AO_DBG_ERR("Invalid key");
        (void)0;
    }
    txNrRef = declareConfiguration<int>(key, 0, AO_TXPROC_FN, false, false, true, false);
    if (!txNrRef || *txNrRef < 0) {
        AO_DBG_ERR("Initialization failure");
    }
}

/*
 * Evaluate the preconditions and triggers. If all are Active, then execute the transaction enable sequence.
 * That is an ordered sequence of preparation steps which must be true before the transaction can start. When
 * the transaction is finished, disable the preparation steps in reversed order.
 * 
 * txEnable is the output variable. See getState() for getting the result.
 */
void TransactionProcess::evaluateProcessSteps(uint txNr) {

#if AO_DBG_LEVEL >= AO_DL_DEBUG
    //print transitions to debug console
    TxEnableState txEnableBefore = txEnable;
#endif

    //Check Tx preconditions: Tx is only possible if all of them are true; search for a false precondition
    auto txPrecondition = TxPrecondition::Active;
    for (auto cond : txPreconditions) {
        if (cond() != TxPrecondition::Active) {
            txPrecondition = TxPrecondition::Inactive;
            break;
        }
    }

    //Check Tx triggers: all of them need to be true to trigger a tx; prepare that search here
    auto txTrigger = txTriggers.empty() ? TxTrigger::Inactive : TxTrigger::Active;
    if (txPrecondition != TxPrecondition::Active) { //Only trigger a tx if the preconditions are met
        txTrigger = TxTrigger::Inactive;
    }

    //Check if the current process is obsolete
    if (txNrRef && (uint) *txNrRef != txNr) {
        txTrigger = TxTrigger::Inactive;
    }

    //Determine if
    // - No trigger is active      -> activeTriggerExists = false, txTrigger = Inactive
    // - All triggers are active   -> activeTriggerExists = true,  txTrigger = Active
    // - Some are active, some not -> activeTriggerExists = true,  txTrigger = Inactive
    activeTriggerExists = false;
    for (auto trigger = txTriggers.begin(); trigger != txTriggers.end(); trigger++) {
        auto result = trigger->operator()();
        if (result == TxTrigger::Active) {
            activeTriggerExists = true;
        } else {
            txTrigger = TxTrigger::Inactive;
        }
    }

    //Also check if all devices on the charger are enabled for the Tx
    if (txTrigger == TxTrigger::Active) { //Search for an unready device
        txEnable = TxEnableState::Active;

        for (auto step = txEnableSequence.rbegin(); step != txEnableSequence.rend(); step++) {
            auto result = step->operator()(TxTrigger::Active);
            if (result != TxEnableState::Active) {
                txEnable = TxEnableState::Pending;
                break;
            }
        }
    } else { //Search for a device that is still enabled
        txEnable = TxEnableState::Inactive;

        for (auto step = txEnableSequence.begin(); step != txEnableSequence.end(); step++) {
            auto result = step->operator()(TxTrigger::Inactive);
            if (result != TxEnableState::Inactive) {
                txEnable = TxEnableState::Pending;
                break;
            }
        }
    }

    //If the current process is obsolete: Check if updating the tx reference is possible
    if (txNrRef && (uint) *txNrRef != txNr) {
        if (txEnable == TxEnableState::Inactive) {
            AO_DBG_DEBUG("Upgrade to next tx: %u", txNr);
            *txNrRef = txNr;
            configuration_save();
        }
    }

#if AO_DBG_LEVEL >= AO_DL_DEBUG
    if (txEnableBefore != txEnable) {
        AO_DBG_DEBUG("Transition from %s to %s",
                    txEnableBefore == TxEnableState::Active ? "Active" : txEnableBefore == TxEnableState::Inactive ? "Inactive" : "Pending",
                    txEnable == TxEnableState::Active ? "Active" : txEnable == TxEnableState::Inactive ? "Inactive" : "Pending");
    }
#endif

}

uint TransactionProcess::getTxNrRef() {
    if (txNrRef) {
        return (uint) *txNrRef;
    }
    return 0;
}
