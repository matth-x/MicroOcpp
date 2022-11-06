// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef OPERATIONSQUEUE_H
#define OPERATIONSQUEUE_H

#include <ArduinoOcpp/Core/OperationStore.h>

#include <memory>
#include <deque>
#include <functional>

namespace ArduinoOcpp {

class FilesystemAdapter;
class OcppOperation;
class OcppModel;

class OperationsQueue {
private:
    OperationStore opStore;
    std::shared_ptr<OcppModel> baseModel;

    std::unique_ptr<OcppOperation> head;
    std::deque<std::unique_ptr<OcppOperation>> tailCache;
public:

    OperationsQueue(std::shared_ptr<OcppModel> baseModel, std::shared_ptr<FilesystemAdapter> filesystem);
    ~OperationsQueue();

    OcppOperation *front();
    void pop_front();

    void initiate(std::unique_ptr<OcppOperation> op);

    std::deque<std::unique_ptr<OcppOperation>>::iterator begin_tail(); //iterator for all cached elements except head
    std::deque<std::unique_ptr<OcppOperation>>::iterator end_tail();
    std::deque<std::unique_ptr<OcppOperation>>::iterator erase_tail(std::deque<std::unique_ptr<OcppOperation>>::iterator el);
    void drop_if(std::function<bool(std::unique_ptr<OcppOperation>&)> pred); //drops operations from this queue where pred(operation) == true. Executes pred in order

};

}

#endif
