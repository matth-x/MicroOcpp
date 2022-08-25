// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Tasks/Transactions/OrderedOperationsQueue.h>
#include <ArduinoOcpp/Tasks/Transactions/TransactionSequence.h>
#include <ArduinoOcpp/Debug.h>

using namespace ArduinoOcpp;

void OrderedOperationsQueue::addOcppOperation(uint seqNr, std::unique_ptr<OcppOperation> op) {
    operations.push_back(std::pair<uint, std::unique_ptr<OcppOperation>>{seqNr, std::move(op)});
}

void OrderedOperationsQueue::sort(uint seqEnd) {
    std::sort(operations.begin(), operations.end(), [seqEnd] (const std::pair<uint, std::unique_ptr<OcppOperation>>& a, const std::pair<uint, std::unique_ptr<OcppOperation>>& b) {
        uint da = (seqEnd - a.first + MAX_TXEVENT_CNT) % MAX_TXEVENT_CNT; //distance between a to seqEnd
        uint db = (seqEnd - b.first + MAX_TXEVENT_CNT) % MAX_TXEVENT_CNT;
        return da > db; //sort descending by distance to seqEnd <=> sort ascending by seqNr
    });
}

void OrderedOperationsQueue::moveTo(std::vector<std::unique_ptr<OcppOperation>>& dst) {
    for (auto el = operations.begin(); el != operations.end(); el++) {
        dst.emplace_back(std::move(el->second));
    }

    operations.clear();
}
