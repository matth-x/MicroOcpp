// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <limits>

#include <MicroOcpp/Core/RequestQueue.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/OcppError.h>
#include <MicroOcpp/Core/OperationRegistry.h>
#include <MicroOcpp/Operations/StatusNotification.h>

#include <MicroOcpp/Debug.h>

size_t removePayload(const char *src, size_t src_size, char *dst, size_t dst_size);

using namespace MicroOcpp;

VolatileRequestQueue::VolatileRequestQueue(unsigned int priority) : MemoryManaged("VolatileRequestQueue"), priority{priority} {

}

VolatileRequestQueue::~VolatileRequestQueue() = default;

void VolatileRequestQueue::loop() {

    /*
     * Drop timed out operations
     */
    size_t i = 0;
    while (i < len) {
        size_t index = (front + i) % MO_REQUEST_CACHE_MAXSIZE;
        auto& request = requests[index];

        if (request->isTimeoutExceeded()) {
            MO_DBG_INFO("operation timeout: %s", request->getOperationType());
            request->executeTimeout();

            if (index == front) {
                requests[front].reset();
                front = (front + 1) % MO_REQUEST_CACHE_MAXSIZE;
                len--;
            } else {
                requests[index].reset();
                for (size_t i = (index + MO_REQUEST_CACHE_MAXSIZE - front) % MO_REQUEST_CACHE_MAXSIZE; i < len - 1; i++) {
                    requests[(front + i) % MO_REQUEST_CACHE_MAXSIZE] = std::move(requests[(front + i + 1) % MO_REQUEST_CACHE_MAXSIZE]);
                }
                len--;
            }
        } else {
            i++;
        }
    }
}

unsigned int VolatileRequestQueue::getFrontRequestOpNr() {
    if (len == 0) {
        return NoOperation;
    }

    return priority;
}

std::unique_ptr<Request> VolatileRequestQueue::fetchFrontRequest() {
    if (len == 0) {
        return nullptr;
    }

    std::unique_ptr<Request> result = std::move(requests[front]);
    front = (front + 1) % MO_REQUEST_CACHE_MAXSIZE;
    len--;

    MO_DBG_VERBOSE("front %zu len %zu", front, len);

    return result;
}

bool VolatileRequestQueue::pushRequestBack(std::unique_ptr<Request> request) {

    // Don't queue up multiple StatusNotification messages for the same connectorId
    if (strcmp(request->getOperationType(), "StatusNotification") == 0)
    {
        size_t i = 0;
        while (i < len) {
            size_t index = (front + i) % MO_REQUEST_CACHE_MAXSIZE;

            if (strcmp(requests[index]->getOperationType(), "StatusNotification")!= 0)
            {
                i++;
                continue;
            }
            auto new_status_notification = static_cast<Ocpp16::StatusNotification*>(request->getOperation());
            auto old_status_notification = static_cast<Ocpp16::StatusNotification*>(requests[index]->getOperation());
            if (old_status_notification->getConnectorId() == new_status_notification->getConnectorId()) {
                requests[index].reset();
                for (size_t i = (index + MO_REQUEST_CACHE_MAXSIZE - front) % MO_REQUEST_CACHE_MAXSIZE; i < len - 1; i++) {
                    requests[(front + i) % MO_REQUEST_CACHE_MAXSIZE] = std::move(requests[(front + i + 1) % MO_REQUEST_CACHE_MAXSIZE]);
                }
                len--;
            } else {
                i++;
            }
        }
    }

    if (len >= MO_REQUEST_CACHE_MAXSIZE) {
        MO_DBG_INFO("Drop cached operation (cache full): %s", requests[front]->getOperationType());
        requests[front]->executeTimeout();
        requests[front].reset();
        front = (front + 1) % MO_REQUEST_CACHE_MAXSIZE;
        len--;
    }

    requests[(front + len) % MO_REQUEST_CACHE_MAXSIZE] = std::move(request);
    len++;
    return true;
}

RequestQueue::RequestQueue(Connection& connection, OperationRegistry& operationRegistry)
            : MemoryManaged("RequestQueue"), connection(connection), operationRegistry(operationRegistry) {

    ReceiveTXTcallback callback = [this] (const char *payload, size_t length) {
        return this->receiveMessage(payload, length);
    };
    
    connection.setReceiveTXTcallback(callback);

    memset(sendQueues, 0, sizeof(sendQueues));
    addSendQueue(&defaultSendQueue);
    addSendQueue(&preBootSendQueue);
}

void RequestQueue::loop() {

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
    preBootSendQueue.loop();

    if (!connection.isConnected()) {
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
    
            bool success = connection.sendTXT(out.c_str(), out.length());

            if (success) {
                MO_DBG_TRAFFIC_OUT(out.c_str());
                recvReqFront.reset();
            }

            return;
        } //else: There will be another attempt to send this conf message in a future loop call
    }

    /**
     * Send pending req message
     */

    if (!sendReqFront) {

        unsigned int minOpNr = RequestEmitter::NoOperation;
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

            bool success = connection.sendTXT(out.c_str(), out.length());

            if (success) {
                MO_DBG_TRAFFIC_OUT(out.c_str());
                sendReqFront->setRequestSent(); //mask as sent and wait for response / timeout
            }

            return;
        }
    }
}

void RequestQueue::sendRequest(std::unique_ptr<Request> op){
    defaultSendQueue.pushRequestBack(std::move(op));
}

void RequestQueue::sendRequestPreBoot(std::unique_ptr<Request> op){
    preBootSendQueue.pushRequestBack(std::move(op));
}

void RequestQueue::addSendQueue(RequestEmitter* sendQueue) {
    for (size_t i = 0; i < MO_NUM_REQUEST_QUEUES; i++) {
        if (!sendQueues[i]) {
            sendQueues[i] = sendQueue;
            return;
        }
    }
    MO_DBG_ERR("exceeded sendQueue capacity");
}

unsigned int RequestQueue::getNextOpNr() {
    return nextOpNr++;
}

bool RequestQueue::receiveMessage(const char* payload, size_t length) {

    MO_DBG_TRAFFIC_IN((int) length, payload);

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
                    auto op = makeRequest(new MsgBufferExceeded(MO_MAX_JSON_CAPACITY, length));
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
void RequestQueue::receiveResponse(JsonArray json) {

    if (!sendReqFront || !sendReqFront->receiveResponse(json)) {
        MO_DBG_WARN("Received response doesn't match pending operation");
    }

    sendReqFront.reset();
}

void RequestQueue::receiveRequest(JsonArray json) {
    auto op = operationRegistry.deserializeOperation(json[2] | "UNDEFINED");
    if (op == nullptr) {
        MO_DBG_WARN("OOM");
        return;
    }
    receiveRequest(json, std::move(op));
}

void RequestQueue::receiveRequest(JsonArray json, std::unique_ptr<Request> op) {
    op->receiveRequest(json); //execute the operation
    recvQueue.pushRequestBack(std::move(op)); //enqueue so loop() plans conf sending
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
size_t removePayload(const char *src, size_t src_size, char *dst, size_t dst_size) {
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
