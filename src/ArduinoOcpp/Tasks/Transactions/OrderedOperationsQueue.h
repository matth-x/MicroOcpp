// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef OPERATIONSQUEUE_H
#define OPERATIONSQUEUE_H

#include <vector>
#include <memory>
#include <ArduinoOcpp/Core/OcppOperation.h>

namespace ArduinoOcpp {

class OrderedOperationsQueue {
private:
    std::vector<std::pair<uint, std::unique_ptr<OcppOperation>>> operations;
public:

    void addOcppOperation(uint seqNr, std::unique_ptr<OcppOperation> op);

    void sort(uint seqEnd); //sort and take seqEnd as largest order number

    void moveTo(std::vector<std::unique_ptr<OcppOperation>>& dst); //move contents of operations to dst
};

}

#endif
