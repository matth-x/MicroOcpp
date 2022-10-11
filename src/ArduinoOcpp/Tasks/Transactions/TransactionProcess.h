// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef TRANSACTIONPROCESS_H
#define TRANSACTIONPROCESS_H

#include <ArduinoOcpp/Tasks/Transactions/TransactionPrerequisites.h>
#include <ArduinoOcpp/Core/Configuration.h>

#include <vector>
#include <functional>

namespace ArduinoOcpp {

class TransactionProcess {
private:
    
    std::vector<std::function<TxPrecondition()>> txPreconditions;
    std::vector<std::function<TxTrigger()>> txTriggers;
    std::vector<std::function<TxEnableState(TxTrigger)>> txEnableSequence;

    bool activeTriggerExists = false;
    TxEnableState txEnable {TxEnableState::Inactive}; // = Result of Trigger and Enable Sequence

    std::shared_ptr<Configuration<int>> txNrRef;
    
public:

    TransactionProcess(uint connectorId);

    void addPrecondition(std::function<TxPrecondition()> fn) {txPreconditions.push_back(fn);}

    void addTrigger(std::function<TxTrigger()> fn) {txTriggers.push_back(fn);}

    void addEnableStep(std::function<TxEnableState(TxTrigger)> fn) {txEnableSequence.push_back(fn);}

    void evaluateProcessSteps(uint txNr);

    bool existsActiveTrigger() {return activeTriggerExists;}
    TxEnableState getState() {return txEnable;}

    uint getTxNrRef();
};

}

#endif
