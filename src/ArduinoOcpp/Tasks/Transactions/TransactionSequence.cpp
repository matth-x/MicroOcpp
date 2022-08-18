// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Tasks/Transactions/TransactionStore.h>
#include <ArduinoOcpp/Core/FilesystemUtils.h>

#include <ArduinoOcpp/Debug.h>

#define AO_TXSEQ_FN AO_FILENAME_PREFIX "/txseq.cnf"

using namespace ArduinoOcpp;

TransactionSequence::TransactionSequence() {

    seqEnd = declareConfiguration<int>("AO_SEQEND", 0, AO_TXSEQ_FN, false, false, true, false);
}

uint TransactionSequence::getSeqEnd() {
    return *seqEnd;
}

uint TransactionSequence::reserveSeqNr() {
    uint seq = (uint) *seqEnd;
    uint seqIncr = (seq + 1U) % MAX_TXEVENT_CNT;
    *seqEnd = seqIncr;
    configuration_save();
    return seq;
}
