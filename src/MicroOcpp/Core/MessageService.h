// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_MESSAGESERVICE_H
#define MO_MESSAGESERVICE_H

#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Core/RequestQueue.h>
#include <MicroOcpp/Core/Memory.h>

#include <ArduinoJson.h>

#ifndef MO_REQUEST_CACHE_MAXSIZE
#define MO_REQUEST_CACHE_MAXSIZE 10
#endif

#ifndef MO_NUM_REQUEST_QUEUES
#define MO_NUM_REQUEST_QUEUES 10
#endif

namespace MicroOcpp {

class Context;
class Operation;

struct OperationCreator {
    const char *operationType = nullptr;
    Operation* (*create)(Context& context) = nullptr;
};

struct CustomOperationCreator {
    const char *operationType = nullptr;
    void *userData = nullptr;
    void (*onRequest)(const char *operationType, const char *payloadJson, int *userStatus, void *userData) = nullptr;
    int (*writeResponse)(const char *operationType, char *buf, size_t size, int userStatus, void *userData) = nullptr;
};

struct OperationListener {
    const char *operationType = nullptr;
    void *userData = nullptr;
    void (*onEvent)(const char *operationType, const char *payloadJson, void *userData) = nullptr;
};

class Connection;

class MessageService : public MemoryManaged {
private:
    Context& context;
    Connection *connection = nullptr;

    RequestQueue* sendQueues [MO_NUM_REQUEST_QUEUES] {nullptr};
    VolatileRequestQueue defaultSendQueue;
    VolatileRequestQueue *preBootSendQueue = nullptr;
    std::unique_ptr<Request> sendReqFront;

    VolatileRequestQueue recvQueue;
    std::unique_ptr<Request> recvReqFront;

    Vector<OperationCreator> operationRegistry;
    Vector<CustomOperationCreator> operationRegistry2;
    Vector<OperationListener> receiveRequestListeners;
    Vector<OperationListener> sendConfListeners;

    void receiveRequest(JsonArray json);
    void receiveRequest(JsonArray json, std::unique_ptr<Request> op);
    void receiveResponse(JsonArray json);
    std::unique_ptr<Request> createRequest(const char *operationType);

    unsigned long sockTrackLastConnected = 0;

    unsigned int nextOpNr = 10; //Nr 0 - 9 reservered for internal purposes
public:
    MessageService(Context& context);

    bool setup();

    void loop(); //polls all reqQueues and decides which request to send (if any)

    // send a Request to the OCPP server
    bool sendRequest(std::unique_ptr<Request> request); //send an OCPP operation request to the server; adds request to default queue
    bool sendRequestPreBoot(std::unique_ptr<Request> request); //send an OCPP operation request to the server; adds request to preBootQueue
    
    // handle Requests from the OCPP Server
    bool registerOperation(const char *operationType, Operation* (*createOperationCb)(Context& context));
    bool registerOperation(const char *operationType, void (*onRequest)(const char *operationType, const char *payloadJson, int *userStatus, void *userData), int (*writeResponse)(const char *operationType, char *buf, size_t size, int userStatus, void *userData), void *userData = nullptr);
    bool setOnReceiveRequest(const char *operationType, void (*onRequest)(const char *operationType, const char *payloadJson, void *userData), void *userData = nullptr);
    bool setOnSendConf(const char *operationType, void (*onConfirmation)(const char *operationType, const char *payloadJson, void *userData), void *userData = nullptr);

    // process message from server ("message" = serialized Request or Confirmation)
    bool receiveMessage(const char* payload, size_t length);

    // Outgoing Requests are handled in separate queues.
    void addSendQueue(RequestQueue* sendQueue);
    void setPreBootSendQueue(VolatileRequestQueue *preBootQueue);

    unsigned int getNextOpNr();
};

} //namespace MicroOcpp
#endif
