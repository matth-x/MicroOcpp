// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_REQUESTQUEUE_H
#define MO_REQUESTQUEUE_H

#include <limits>

#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/Memory.h>

#include <memory>
#include <ArduinoJson.h>

#ifndef MO_REQUEST_CACHE_MAXSIZE
#define MO_REQUEST_CACHE_MAXSIZE 10
#endif

#ifndef MO_NUM_REQUEST_QUEUES
#define MO_NUM_REQUEST_QUEUES 5
#endif

namespace MicroOcpp {

class Connection;
class OperationRegistry;
class Request;

class RequestEmitter {
public:
    static const unsigned int NoOperation = std::numeric_limits<unsigned int>::max();

    virtual unsigned int getFrontRequestOpNr() = 0; //return OpNr of front request or NoOperation if queue is empty
    virtual std::unique_ptr<Request> fetchFrontRequest() = 0;
};

class VolatileRequestQueue : public RequestEmitter, public MemoryManaged {
private:
    std::unique_ptr<Request> requests [MO_REQUEST_CACHE_MAXSIZE];
    size_t front = 0, len = 0;
public:
    VolatileRequestQueue();
    ~VolatileRequestQueue();
    void loop();

    unsigned int getFrontRequestOpNr() override;
    std::unique_ptr<Request> fetchFrontRequest() override;

    bool pushRequestBack(std::unique_ptr<Request> request);
};

class RequestQueue : public MemoryManaged {
private:
    Connection& connection;
    OperationRegistry& operationRegistry;

    RequestEmitter* sendQueues [MO_NUM_REQUEST_QUEUES];
    VolatileRequestQueue defaultSendQueue;
    VolatileRequestQueue *preBootSendQueue = nullptr;
    std::unique_ptr<Request> sendReqFront;

    VolatileRequestQueue recvQueue;
    std::unique_ptr<Request> recvReqFront;

    bool receiveMessage(const char* payload, size_t length); //receive from  server: either a request or response
    void receiveRequest(JsonArray json);
    void receiveRequest(JsonArray json, std::unique_ptr<Request> op);
    void receiveResponse(JsonArray json);

    unsigned long sockTrackLastConnected = 0;

    unsigned int nextOpNr = 10; //Nr 0 - 9 reservered for internal purposes
public:
    RequestQueue() = delete;
    RequestQueue(const RequestQueue&) = delete;
    RequestQueue(const RequestQueue&&) = delete;

    RequestQueue(Connection& connection, OperationRegistry& operationRegistry);

    void loop(); //polls all reqQueues and decides which request to send (if any)

    void sendRequest(std::unique_ptr<Request> request); //send an OCPP operation request to the server; adds request to default queue
    void sendRequestPreBoot(std::unique_ptr<Request> request); //send an OCPP operation request to the server; adds request to preBootQueue

    void addSendQueue(RequestEmitter* sendQueue);
    void setPreBootSendQueue(VolatileRequestQueue *preBootQueue);

    unsigned int getNextOpNr();
};

} //end namespace MicroOcpp
#endif
