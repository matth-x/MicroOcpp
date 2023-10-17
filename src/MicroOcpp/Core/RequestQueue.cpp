// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Core/RequestQueue.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>
#include <MicroOcpp/Core/OcppError.h>
#include <MicroOcpp/Core/RequestStore.h>
#include <MicroOcpp/Core/OperationRegistry.h>

#include <MicroOcpp/Debug.h>

size_t removePayload(const char *src, size_t src_size, char *dst, size_t dst_size);

using namespace MicroOcpp;

RequestQueue::RequestQueue(OperationRegistry& operationRegistry, Model *baseModel, std::shared_ptr<FilesystemAdapter> filesystem)
            : operationRegistry(operationRegistry) {
    
    if (filesystem) {
        initiatedRequests.reset(new PersistentRequestQueue(baseModel, filesystem));
    } else {
        initiatedRequests.reset(new VolatileRequestQueue());
    }
}

void RequestQueue::setConnection(Connection& sock) {
    ReceiveTXTcallback callback = [this] (const char *payload, size_t length) {
        return this->receiveMessage(payload, length);
    };
    
    sock.setReceiveTXTcallback(callback);
}

void RequestQueue::loop(Connection& ocppSock) {

    /*
     * Sort out timed out operations
     */
    initiatedRequests->drop_if([] (std::unique_ptr<Request>& op) -> bool {
        bool timed_out = op->isTimeoutExceeded();
        if (timed_out) {
            MO_DBG_INFO("operation timeout: %s", op->getOperationType());
            op->executeTimeout();
        }
        return timed_out;
    });

    /**
     * Send and dequeue a pending confirmation message, if existing. If the first operation is awaiting,
     * try with the subsequent operations.
     * 
     * If a message has been sent, terminate this loop() function.
     */
    for (auto received = receivedRequests.begin(); received != receivedRequests.end(); ++received) {
    
        auto response = (*received)->createResponse();

        if (response) {
            std::string out;
            serializeJson(*response, out);
    
            bool success = ocppSock.sendTXT(out.c_str(), out.length());

            if (success) {
                MO_DBG_TRAFFIC_OUT(out.c_str());
                receivedRequests.erase(received);
            }

            return;
        } //else: There will be another attempt to send this conf message in a future loop call.
          //      Go on with the next element in the queue.
    }

    /**
     * Send pending req message
     */
    auto initedOp = initiatedRequests->front();

    if (!initedOp) {
        //queue empty
        return;
    }

    //check backoff time

    if (initedOp->getTrialNo() == 0) {
        //first trial -> send immediately
        sendBackoffPeriod = 0;
    }

    if (sockTrackLastConnected != ocppSock.getLastConnected()) {
        //connection active (again) -> send immediately
        sendBackoffPeriod = std::min(sendBackoffPeriod, 1000UL);
    }
    sockTrackLastConnected = ocppSock.getLastConnected();

    if (mocpp_tick_ms() - sendBackoffTime < sendBackoffPeriod) {
        //still in backoff period
        return;
    }

    auto request = initedOp->createRequest();

    if (!request) {
        //request not ready yet or OOM
        return;
    }

    //send request
    std::string out;
    serializeJson(*request, out);

    bool success = ocppSock.sendTXT(out.c_str(), out.length());

    if (success) {
        MO_DBG_TRAFFIC_OUT(out.c_str());

        //update backoff time
        sendBackoffTime = mocpp_tick_ms();
        sendBackoffPeriod = std::min(sendBackoffPeriod + BACKOFF_PERIOD_INCREMENT, BACKOFF_PERIOD_MAX);
    }
}

void RequestQueue::sendRequest(std::unique_ptr<Request> op){
    if (!op) {
        MO_DBG_ERR("Called with null. Ignore");
        return;
    }
    
    initiatedRequests->push_back(std::move(op));
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
    
    DynamicJsonDocument doc {0};
    DeserializationError err = DeserializationError::NoMemory;

    while (err == DeserializationError::NoMemory && capacity <= MO_MAX_JSON_CAPACITY) {

        doc = DynamicJsonDocument(capacity);
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

            doc = DynamicJsonDocument(200);
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

    bool success = false;

    initiatedRequests->drop_if(
        [&json, &success] (std::unique_ptr<Request>& operation) {
            bool match = operation->receiveResponse(json);
            if (match) {
                success = true;
                //operation will be deleted by the surrounding drop_if
            }
            return match;
        }); //executes in order and drops every operation where predicate(op) == true

    if (!success) {
        //didn't find matching Request
        if (json[0] == MESSAGE_TYPE_CALLERROR) {
            MO_DBG_DEBUG("Received CALLERROR did not abort a pending operation");
            (void)0;
        } else {
            MO_DBG_WARN("Received response doesn't match any pending operation");
            (void)0;
        }
    }
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
    receivedRequests.push_back(std::move(op)); //enqueue so loop() plans conf sending
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
