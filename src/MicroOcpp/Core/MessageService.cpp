// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#include <MicroOcpp/Core/MessageService.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/OcppError.h>
#include <MicroOcpp/Operations/StatusNotification.h>
#include <MicroOcpp/Operations/CustomOperation.h>
#include <MicroOcpp/Context.h>

#include <MicroOcpp/Debug.h>

namespace MicroOcpp {
size_t removePayload(const char *src, size_t src_size, char *dst, size_t dst_size);
}

using namespace MicroOcpp;

MessageService::MessageService(Context& context) :
            MemoryManaged("MessageService"),
            context(context),
            operationRegistry(makeVector<OperationCreator>(getMemoryTag())), 
            operationRegistry2(makeVector<CustomOperationCreator>(getMemoryTag())),
            receiveRequestListeners(makeVector<OperationListener>(getMemoryTag())),
            sendConfListeners(makeVector<OperationListener>(getMemoryTag())) {

    addSendQueue(&defaultSendQueue);
}

bool MessageService::setup() {
    connection = context.getConnection();
    if (!connection) {
        MO_DBG_ERR("need to set Connection before MessageService setup");
        return false;
    }
    return true;
}

void MessageService::loop() {

    /*
     * Check if front request timed out
     */
    if (sendReqFront && sendReqFront->isTimeoutExceeded()) {
        MO_DBG_INFO("operation timeout: %s", sendReqFront->getOperationType());
        sendReqFront->executeTimeout();
        sendReqFront.reset();
    }

    if (recvReqFront && recvReqFront->isTimeoutExceeded()) {
        MO_DBG_INFO("operation timeout: %s", recvReqFront->getOperationType());
        recvReqFront->executeTimeout();
        recvReqFront.reset();
    }

    defaultSendQueue.loop();

    if (!connection->isConnected()) {
        return;
    }

    /**
     * Send and dequeue a pending confirmation message, if existing
     * 
     * If a message has been sent, terminate this loop() function.
     */

    if (!recvReqFront) {
        recvReqFront = recvQueue.fetchFrontRequest();
    }

    if (recvReqFront) {

        auto response = initJsonDoc(getMemoryTag());
        auto ret = recvReqFront->createResponse(response);

        if (ret == Request::CreateResponseResult::Success) {
            auto out = makeString(getMemoryTag());
            serializeJson(response, out);
    
            bool success = connection->sendTXT(out.c_str(), out.length());

            if (success) {
                MO_DBG_DEBUG("Send %s", out.c_str());
                recvReqFront.reset();
            }

            return;
        } //else: There will be another attempt to send this conf message in a future loop call
    }

    /**
     * Send pending req message
     */

    if (!sendReqFront) {

        unsigned int minOpNr = RequestQueue::NoOperation;
        size_t index = MO_NUM_REQUEST_QUEUES;
        for (size_t i = 0; i < MO_NUM_REQUEST_QUEUES && sendQueues[i]; i++) {
            auto opNr = sendQueues[i]->getFrontRequestOpNr();
            if (opNr < minOpNr) {
                minOpNr = opNr;
                index = i;
            }
        }

        if (index < MO_NUM_REQUEST_QUEUES) {
            sendReqFront = sendQueues[index]->fetchFrontRequest();
        }
    }

    if (sendReqFront && !sendReqFront->isRequestSent()) {

        auto request = initJsonDoc(getMemoryTag());
        auto ret = sendReqFront->createRequest(request);

        if (ret == Request::CreateRequestResult::Success) {

            //send request
            auto out = makeString(getMemoryTag());
            serializeJson(request, out);

            bool success = connection->sendTXT(out.c_str(), out.length());

            if (success) {
                MO_DBG_DEBUG("Send %s", out.c_str());
                sendReqFront->setRequestSent(); //mask as sent and wait for response / timeout
            }

            return;
        }
    }
}

bool MessageService::sendRequest(std::unique_ptr<Request> op){
    return defaultSendQueue.pushRequestBack(std::move(op));
}

bool MessageService::sendRequestPreBoot(std::unique_ptr<Request> op){
    if (!preBootSendQueue) {
        MO_DBG_ERR("did not set PreBoot queue");
        return false;
    }
    return preBootSendQueue->pushRequestBack(std::move(op));
}

void MessageService::addSendQueue(RequestQueue* sendQueue) {
    for (size_t i = 0; i < MO_NUM_REQUEST_QUEUES; i++) {
        if (!sendQueues[i]) {
            sendQueues[i] = sendQueue;
            return;
        }
    }
    MO_DBG_ERR("exceeded sendQueue capacity");
}

void MessageService::setPreBootSendQueue(VolatileRequestQueue *preBootQueue) {
    this->preBootSendQueue = preBootQueue;
    addSendQueue(preBootQueue);
}

unsigned int MessageService::getNextOpNr() {
    return nextOpNr++;
}

namespace MicroOcpp {

template <class T>
T *getEntry(Vector<T>& listeners, const char *operationType) {
    for (size_t i = 0; i < listeners.size(); i++) {
        if (!strcmp(listeners[i].operationType, operationType)) {
            return &listeners[i];
        }
    }
    return nullptr;
}

template <class T>
void removeEntry(Vector<T>& listeners, const char *operationType) {
    for (auto it = listeners.begin(); it != listeners.end();) {
        if (!strcmp(operationType, it->operationType)) {
            it = listeners.erase(it);
        } else {
            it++;
        }
    }
}

template <class T>
bool appendEntry(Vector<T>& listeners, const T& el) {
    bool capacity = listeners.size() + 1;
    listeners.reserve(capacity);
    if (listeners.capacity() < capacity) {
        MO_DBG_ERR("OOM");
        return false;
    }

    listeners.push_back(el);
    return true;
}

} //namespace MicroOcpp

bool MessageService::registerOperation(const char *operationType, Operation* (*createOperationCb)(Context& context)) {

    bool exists = false;

    exists |= (getEntry(operationRegistry, operationType) != nullptr);
    exists |= (getEntry(operationRegistry2, operationType) != nullptr);

    if (exists) {
        MO_DBG_DEBUG("operation creator %s already exists - do not replace", operationType);
        return true;
    }

    OperationCreator entry;
    entry.operationType = operationType;
    entry.create = createOperationCb;

    MO_DBG_DEBUG("registered operation %s", operationType);
    return appendEntry(operationRegistry, entry);
}

bool MessageService::registerOperation(const char *operationType, void (*onRequest)(const char *operationType, const char *payloadJson, int *userStatus, void *userData), int (*writeResponse)(const char *operationType, char *buf, size_t size, int userStatus, void *userData), void *userData) {

    bool exists = false;

    exists |= (getEntry(operationRegistry, operationType) != nullptr);
    exists |= (getEntry(operationRegistry2, operationType) != nullptr);

    if (exists) {
        MO_DBG_DEBUG("operation creator %s already exists - do not replace", operationType);
        return true;
    }

    CustomOperationCreator entry;
    entry.operationType = operationType;
    entry.userData = userData;
    entry.onRequest = onRequest;
    entry.writeResponse = writeResponse;

    MO_DBG_DEBUG("registered operation %s", operationType);
    return appendEntry(operationRegistry2, entry);
}

bool MessageService::setOnReceiveRequest(const char *operationType, void (*onRequest)(const char *operationType, const char *payloadJson, void *userData), void *userData) {
    bool exists = (getEntry(receiveRequestListeners, operationType)) != nullptr;
    if (exists) {
        MO_DBG_DEBUG("operation onRequest %s already exists - do not replace", operationType);
        return true;
    }
    MO_DBG_DEBUG("set onRequest %s", operationType);
    OperationListener listener;
    listener.operationType = operationType;
    listener.onEvent = onRequest;
    listener.userData = userData;
    return appendEntry(receiveRequestListeners, listener);
}

bool MessageService::setOnSendConf(const char *operationType, void (*onConfirmation)(const char *operationType, const char *payloadJson, void *userData), void *userData) {
    bool exists = (getEntry(sendConfListeners, operationType)) != nullptr;
    if (exists) {
        MO_DBG_DEBUG("operation onConfirmation %s already exists - do not replace", operationType);
        return true;
    }
    MO_DBG_DEBUG("set onConfirmation %s", operationType);
    OperationListener listener;
    listener.operationType = operationType;
    listener.onEvent = onConfirmation;
    listener.userData = userData;
    return appendEntry(sendConfListeners, listener);
}

std::unique_ptr<Request> MessageService::createRequest(const char *operationType) {

    Operation *operation = nullptr;
    if (auto entry = getEntry(operationRegistry, operationType)) {
        operation = entry->create(context);
        if (!operation) {
            MO_DBG_ERR("create operation failure");
            return nullptr;
        }
    }

    if (!operation) {
        if (auto entry = getEntry(operationRegistry2, operationType)) {
            auto customOperation = new CustomOperation();
            if (!customOperation) {
                MO_DBG_ERR("OOM");
                return nullptr;
            }

            if (!customOperation->setupCsmsInitiated(
                        entry->operationType,
                        entry->onRequest,
                        entry->writeResponse,
                        entry->userData)) {
                MO_DBG_ERR("create operation failure");
                delete customOperation;
                return nullptr;
            }

            operation = static_cast<Operation*>(customOperation);
        }
    }

    if (!operation) {
        auto notImplemented = new NotImplemented();
        if (!notImplemented) {
            MO_DBG_ERR("OOM");
            return nullptr;
        }
        auto request = makeRequest(context, notImplemented);
        if (!request) {
            MO_DBG_ERR("OOM");
            return nullptr;
        }
        return request;
    }

    auto request = makeRequest(context, operation);
    if (!request) {
        MO_DBG_ERR("OOM");
        return nullptr;
    }

    if (auto entry = getEntry(receiveRequestListeners, operationType)) {
        request->setOnReceiveRequest(entry->onEvent, entry->userData);
    }

    if (auto entry = getEntry(sendConfListeners, operationType)) {
        request->setOnSendConf(entry->onEvent, entry->userData);
    }

    return request;
}

bool MessageService::receiveMessage(const char* payload, size_t length) {

    MO_DBG_DEBUG("Recv %.*s", (int)length, payload);

    size_t capacity_init = (3 * length) / 2;

    //capacity = ceil capacity_init to the next power of two; should be at least 128

    size_t capacity = 128;
    while (capacity < capacity_init && capacity < MO_MAX_JSON_CAPACITY) {
        capacity *= 2;
    }
    if (capacity > MO_MAX_JSON_CAPACITY) {
        capacity = MO_MAX_JSON_CAPACITY;
    }
    
    auto doc = initJsonDoc(getMemoryTag());
    DeserializationError err = DeserializationError::NoMemory;

    while (err == DeserializationError::NoMemory && capacity <= MO_MAX_JSON_CAPACITY) {

        doc = initJsonDoc(getMemoryTag(), capacity);
        err = deserializeJson(doc, payload, length);

        capacity *= 2;
    }

    bool success = false;

    switch (err.code()) {
        case DeserializationError::Ok: {
            int messageTypeId = doc[0] | -1;

            if (messageTypeId == MESSAGE_TYPE_CALL) {
                receiveRequest(doc.as<JsonArray>());      
                success = true;
            } else if (messageTypeId == MESSAGE_TYPE_CALLRESULT ||
                    messageTypeId == MESSAGE_TYPE_CALLERROR) {
                receiveResponse(doc.as<JsonArray>());
                success = true;
            } else {
                MO_DBG_WARN("Invalid OCPP message! (though JSON has successfully been deserialized)");
            }
            break; 
        }
        case DeserializationError::InvalidInput:
            MO_DBG_WARN("Invalid input! Not a JSON");
            break;
        case DeserializationError::NoMemory: {
            MO_DBG_WARN("incoming operation exceeds buffer capacity. Input length = %zu, max capacity = %d", length, MO_MAX_JSON_CAPACITY);

            /*
                * If websocket input is of message type MESSAGE_TYPE_CALL, send back a message of type MESSAGE_TYPE_CALLERROR.
                * Then the communication counterpart knows that this operation failed.
                * If the input type is MESSAGE_TYPE_CALLRESULT, then abort the operation to avoid getting stalled.
                */

            doc = initJsonDoc(getMemoryTag(), 200);
            char onlyRpcHeader[200];
            size_t onlyRpcHeader_len = removePayload(payload, length, onlyRpcHeader, sizeof(onlyRpcHeader));
            DeserializationError err2 = deserializeJson(doc, onlyRpcHeader, onlyRpcHeader_len);
            if (err2.code() == DeserializationError::Ok) {
                int messageTypeId = doc[0] | -1;
                if (messageTypeId == MESSAGE_TYPE_CALL) {
                    success = true;
                    auto op = makeRequest(context, new MsgBufferExceeded(MO_MAX_JSON_CAPACITY, length));
                    receiveRequest(doc.as<JsonArray>(), std::move(op));
                } else if (messageTypeId == MESSAGE_TYPE_CALLRESULT ||
                            messageTypeId == MESSAGE_TYPE_CALLERROR) {
                    success = true;
                    MO_DBG_WARN("crop incoming response");
                    receiveResponse(doc.as<JsonArray>());
                }
            }
            break;
        }
        default:
            MO_DBG_WARN("Deserialization failed: %s", err.c_str());
            break;
    }

    return success;
}

/**
 * call conf() on each element of the queue. Start with first element. On successful message delivery,
 * delete the element from the list. Try all the pending OCPP Operations until the right one is found.
 * 
 * This function could result in improper behavior in Charging Stations, because messages are not
 * guaranteed to be received and therefore processed in the right order.
 */
void MessageService::receiveResponse(JsonArray json) {

    if (!sendReqFront || !sendReqFront->receiveResponse(json)) {
        MO_DBG_WARN("Received response doesn't match pending operation");
    }

    sendReqFront.reset();
}

void MessageService::receiveRequest(JsonArray json) {
    if (json.size() <= 2 || !json[2].is<const char*>()) {
        MO_DBG_ERR("malformatted request");
        return;
    }
    auto request = createRequest(json[2]);
    if (request == nullptr) {
        MO_DBG_WARN("OOM");
        return;
    }
    receiveRequest(json, std::move(request));
}

void MessageService::receiveRequest(JsonArray json, std::unique_ptr<Request> request) {
    request->receiveRequest(json); //execute the operation
    recvQueue.pushRequestBack(std::move(request)); //enqueue so loop() plans conf sending
}

/*
 * Tries to recover the Ocpp-Operation header from a broken message.
 * 
 * Example input: 
 * [2, "75705e50-682d-404e-b400-1bca33d41e19", "ChangeConfiguration", {"key":"now the message breaks...
 * 
 * The Json library returns an error code when trying to deserialize that broken message. This
 * function searches for the first occurence of the character '{' and writes "}]" after it.
 * 
 * Example output:
 * [2, "75705e50-682d-404e-b400-1bca33d41e19", "ChangeConfiguration", {}]
 *
 */
size_t MicroOcpp::removePayload(const char *src, size_t src_size, char *dst, size_t dst_size) {
    size_t res_len = 0;
    for (size_t i = 0; i < src_size && i < dst_size-3; i++) {
        if (src[i] == '\0'){
            //no payload found within specified range. Cancel execution
            break;
        }
        dst[i] = src[i];
        if (src[i] == '{') {
            dst[i+1] = '}';
            dst[i+2] = ']';
            res_len = i+3;
            break;
        }
    }
    dst[res_len] = '\0';
    res_len++;
    return res_len;
}
