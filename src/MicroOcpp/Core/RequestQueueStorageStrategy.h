// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef REQUESTQUEUESTORAGESTRATEGY_H
#define REQUESTQUEUESTORAGESTRATEGY_H

#include <MicroOcpp/Core/RequestStore.h>

#include <memory>
#include <deque>
#include <functional>

namespace MicroOcpp {

class FilesystemAdapter;
class Request;
class Model;

class RequestQueueStorageStrategy {
public:
    virtual ~RequestQueueStorageStrategy() = default;

    virtual Request *front() = 0;
    virtual void pop_front() = 0;

    virtual void push_back(std::unique_ptr<Request> op) = 0;

    virtual void drop_if(std::function<bool(std::unique_ptr<Request>&)> pred) = 0; //drops operations from this queue where pred(operation) == true. Executes pred in order
};

class VolatileRequestQueue : public RequestQueueStorageStrategy {
private:
    std::deque<std::unique_ptr<Request>> queue;
public:
    VolatileRequestQueue();
    ~VolatileRequestQueue();

    Request *front() override;
    void pop_front() override;

    void push_back(std::unique_ptr<Request> op) override;

    void drop_if(std::function<bool(std::unique_ptr<Request>&)> pred) override; //drops operations from this queue where pred(operation) == true. Executes pred in order
};

class PersistentRequestQueue : public RequestQueueStorageStrategy {
private:
    RequestStore opStore;
    Model *baseModel;

    std::unique_ptr<Request> head;
    std::deque<std::unique_ptr<Request>> tailCache;
public:

    PersistentRequestQueue(Model *baseModel, std::shared_ptr<FilesystemAdapter> filesystem);
    ~PersistentRequestQueue();

    Request *front() override;
    void pop_front() override;

    void push_back(std::unique_ptr<Request> op) override;

    void drop_if(std::function<bool(std::unique_ptr<Request>&)> pred) override; //drops operations from this queue where pred(operation) == true. Executes pred in order

};

}

#endif
