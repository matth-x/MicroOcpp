// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef TRANSACTIONSEQUENCE_H
#define TRANSACTIONSEQUENCE_H

#include <ArduinoOcpp/Core/Configuration.h>
#include <vector>

#define MAX_TXEVENT_CNT 100000U

namespace ArduinoOcpp {

class TransactionSequence {
private:
    std::shared_ptr<Configuration<int>> seqEnd; //one place after last event
public:
    TransactionSequence();

    uint getSeqEnd();

    uint reserveSeqNr();
};

}

#endif
