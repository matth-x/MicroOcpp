// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
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
public:
    virtual ~OperationsQueue() = default;

    virtual OcppOperation *front() = 0;
    virtual void pop_front() = 0;

    virtual void initiate(std::unique_ptr<OcppOperation> op) = 0;

    virtual std::deque<std::unique_ptr<OcppOperation>>::iterator begin_tail() = 0; //iterator for all cached elements except head
    virtual std::deque<std::unique_ptr<OcppOperation>>::iterator end_tail() = 0;
    virtual std::deque<std::unique_ptr<OcppOperation>>::iterator erase_tail(std::deque<std::unique_ptr<OcppOperation>>::iterator el) = 0;
    virtual void drop_if(std::function<bool(std::unique_ptr<OcppOperation>&)> pred) = 0; //drops operations from this queue where pred(operation) == true. Executes pred in order
};

class VolatileOperationsQueue : public OperationsQueue {
private:
    std::deque<std::unique_ptr<OcppOperation>> queue;
public:
    VolatileOperationsQueue();
    ~VolatileOperationsQueue();

    OcppOperation *front() override;
    void pop_front() override;

    void initiate(std::unique_ptr<OcppOperation> op) override;

    std::deque<std::unique_ptr<OcppOperation>>::iterator begin_tail() override; //iterator for all cached elements except head
    std::deque<std::unique_ptr<OcppOperation>>::iterator end_tail() override;
    std::deque<std::unique_ptr<OcppOperation>>::iterator erase_tail(std::deque<std::unique_ptr<OcppOperation>>::iterator el) override;
    void drop_if(std::function<bool(std::unique_ptr<OcppOperation>&)> pred) override; //drops operations from this queue where pred(operation) == true. Executes pred in order
};

class PersistentOperationsQueue : public OperationsQueue {
private:
    OperationStore opStore;
    std::shared_ptr<OcppModel> baseModel;

    std::unique_ptr<OcppOperation> head;
    std::deque<std::unique_ptr<OcppOperation>> tailCache;
public:

    PersistentOperationsQueue(std::shared_ptr<OcppModel> baseModel, std::shared_ptr<FilesystemAdapter> filesystem);
    ~PersistentOperationsQueue();

    OcppOperation *front() override;
    void pop_front() override;

    void initiate(std::unique_ptr<OcppOperation> op) override;

    std::deque<std::unique_ptr<OcppOperation>>::iterator begin_tail() override; //iterator for all cached elements except head
    std::deque<std::unique_ptr<OcppOperation>>::iterator end_tail() override;
    std::deque<std::unique_ptr<OcppOperation>>::iterator erase_tail(std::deque<std::unique_ptr<OcppOperation>>::iterator el) override;
    void drop_if(std::function<bool(std::unique_ptr<OcppOperation>&)> pred) override; //drops operations from this queue where pred(operation) == true. Executes pred in order

};

}

#endif
