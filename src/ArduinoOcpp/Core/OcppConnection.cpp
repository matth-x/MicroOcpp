// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Core/OcppConnection.h>
#include <ArduinoOcpp/Core/OcppOperation.h>
#include <ArduinoOcpp/Core/OcppSocket.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>
#include <ArduinoOcpp/Core/OcppError.h>
#include <ArduinoOcpp/Core/OperationStore.h>

#include <ArduinoOcpp/Debug.h>

size_t removePayload(const char *src, size_t src_size, char *dst, size_t dst_size);

using namespace ArduinoOcpp;

OcppConnection::OcppConnection(std::shared_ptr<OcppModel> baseModel, std::shared_ptr<FilesystemAdapter> filesystem)
            : baseModel{baseModel}, filesystem{filesystem} {
    
    if (filesystem) {
        initiatedOcppOperations.reset(new PersistentOperationsQueue(baseModel, filesystem));
    } else {
        initiatedOcppOperations.reset(new VolatileOperationsQueue());
    }
}

void OcppConnection::setSocket(OcppSocket& sock) {
    ReceiveTXTcallback callback = [this] (const char *payload, size_t length) {
        return this->receiveMessage(payload, length);
    };
    
    sock.setReceiveTXTcallback(callback);
}

void OcppConnection::loop(OcppSocket& ocppSock) {

    /**
     * Work through the initiatedOcppOperations queue. Start with the first element by calling req() on it. If
     * the operation is (still) pending, it will return false and therefore each other queued element must
     * wait until this operation is over. If an ocppOperation is finished, it returns true on a req() call, 
     * can be dequeued and the following ocppOperation is processed.
     */

    auto inited = initiatedOcppOperations->front();
    if (inited) {
        bool timeout = inited->sendReq(ocppSock); //The only reason to dequeue elements here is when a timeout occurs. Normally
        if (timeout){                                       //the Conf msg processing routine dequeues finished elements
            initiatedOcppOperations->pop_front();
        }
    }

    /*
     * Activate timeout detection on the msgs other than the first in the queue.
     */

    auto cached = initiatedOcppOperations->begin_tail();
    while (cached != initiatedOcppOperations->end_tail()) {
        Timeout *timer = (*cached)->getTimeout();
        if (!timer) {
            ++cached; //no timeouts, nothing to do in this iteration
            continue;
        }
        timer->tick(false); //false: did not send a frame prior to calling tick
        if (timer->isExceeded() &&
                //dropping operations out-of-order is only possible if they do not own an opNr
                (!(*cached)->getStorageHandler() || (*cached)->getStorageHandler()->getOpNr() < 0)) {
            AO_DBG_INFO("Discarding cached due to timeout: %s", (*cached)->getOcppOperationType());
            cached = initiatedOcppOperations->erase_tail(cached);
        } else {
            ++cached;
        }
    }
    
    /**
     * Work through the receivedOcppOperations queue. Start with the first element by calling conf() on it. 
     * If an ocppOperation is finished, it returns true on a conf() call, and is dequeued.
     */

    auto received = receivedOcppOperations.begin();
    while (received != receivedOcppOperations.end()){
        bool success = (*received)->sendConf(ocppSock);
        if (success){
            received = receivedOcppOperations.erase(received);
        } else {
            //There will be another attempt to send this conf message in a future loop call.
            //Go on with the next element in the queue, which is now at receivedOcppOperations[i+1]
            ++received; //TODO review: this makes confs out-of-order. But if the first Op fails because of lacking RAM, this could save the device. 
        }
    }
}

void OcppConnection::initiateOperation(std::unique_ptr<OcppOperation> o){
    if (!o) {
        AO_DBG_ERR("Called with null. Ignore");
        return;
    }
    if (!o->isFullyConfigured()){
        AO_DBG_ERR("Called without the operation being configured and ready to send. Discard operation!");
        return; //o gets destroyed
    }
    
    initiatedOcppOperations->initiate(std::move(o));
}

bool OcppConnection::receiveMessage(const char* payload, size_t length) {

    AO_DBG_TRAFFIC_IN((int) length, payload);
    
    size_t capacity_init = (3 * length) / 2;

    //capacity = ceil capacity_init to the next power of two; should be at least 128

    size_t capacity = 128;
    while (capacity < capacity_init && capacity < AO_MAX_JSON_CAPACITY) {
        capacity *= 2;
    }
    if (capacity > AO_MAX_JSON_CAPACITY) {
        capacity = AO_MAX_JSON_CAPACITY;
    }

    DynamicJsonDocument doc;
    DeserializationError err = DeserializationError::NoMemory;

    while (err == DeserializationError::NoMemory && capacity <= AO_MAX_JSON_CAPACITY) {

        doc = DynamicJsonDocument(capacity);
        err = deserializeJson(doc, payload, length);

        capacity *= 2;
    }

    bool success = false;

    switch (err.code()) {
        case DeserializationError::Ok:
            int messageTypeId = doc[0] | -1;
                switch(messageTypeId) {
                    case MESSAGE_TYPE_CALL:
                    handleReqMessage(doc);      
                    success = true;
                        break;
                    case MESSAGE_TYPE_CALLRESULT:
                    handleConfMessage(doc);
                    success = true;
                        break;
                    case MESSAGE_TYPE_CALLERROR:
                    handleErrMessage(doc);
                    success = true;
                        break;
                    default:
                        AO_DBG_WARN("Invalid OCPP message! (though JSON has successfully been deserialized)");
                        break;
            }
            break;
        case DeserializationError::InvalidInput:
            AO_DBG_WARN("Invalid input! Not a JSON");
            break;
        case DeserializationError::NoMemory:
            {
                AO_DBG_WARN("OOP! Incoming operation exceeds reserved heap. Input length = %zu, max capacity = %d", length, AO_MAX_JSON_CAPACITY);

                /*
                 * If websocket input is of message type MESSAGE_TYPE_CALL, send back a message of type MESSAGE_TYPE_CALLERROR.
                 * Then the communication counterpart knows that this operation failed.
                 * If the input type is MESSAGE_TYPE_CALLRESULT, it can be ignored. This controller will automatically resend the corresponding request message.
                 */

                doc =  DynamicJsonDocument(200);
                char onlyRpcHeader[200];
                size_t onlyRpcHeader_len = removePayload(payload, length, onlyRpcHeader, sizeof(onlyRpcHeader));
                DeserializationError err2 = deserializeJson(*doc, onlyRpcHeader, onlyRpcHeader_len);
                if (err2.code() == DeserializationError::Ok) {
                    int messageTypeId2 = doc[0] | -1;
                    if (messageTypeId2 == MESSAGE_TYPE_CALL) {
                        success = true;
                        auto op = makeOcppOperation(new OutOfMemory(AO_MAX_JSON_CAPACITY, length));
                        handleReqMessage(doc, std::move(op));
                    }
                }
            }
            break;
        default:
            AO_DBG_WARN("Deserialization failed: %s", err.c_str());
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
void OcppConnection::handleConfMessage(JsonDocument& json) {

    bool success = false;

    initiatedOcppOperations->drop_if(
        [&json, &success] (std::unique_ptr<OcppOperation>& operation) {
            bool match = operation->receiveConf(json);
            if (match) {
                success = true;
                //operation will be deleted by the surrounding drop_if
            }
            return match;
        }); //executes in order and drops every operation where predicate(op) == true

    if (!success) {
        //didn't find matching OcppOperation
        AO_DBG_WARN("Received CALLRESULT doesn't match any pending operation");
        (void)0;
    }
}

void OcppConnection::handleReqMessage(JsonDocument& json) {
    auto op = makeFromJson(json);
    if (op == nullptr) {
        AO_DBG_WARN("Couldn't make OppOperation from Request. Ignore request");
        return;
    }
    handleReqMessage(json, std::move(op));
}

void OcppConnection::handleReqMessage(JsonDocument& json, std::unique_ptr<OcppOperation> op) {
    if (op == nullptr) {
        AO_DBG_ERR("Invalid argument");
        return;
    }
    op->setOcppModel(baseModel);
    op->receiveReq(json); //"fire" the operation
    receivedOcppOperations.push_back(std::move(op)); //enqueue so loop() plans conf sending
}

void OcppConnection::handleErrMessage(JsonDocument& json) {

    bool success = false;

    initiatedOcppOperations->drop_if(
        [&json, &success] (std::unique_ptr<OcppOperation>& operation) {
            bool match = operation->receiveError(json);
            if (match) {
                success = true;
                //operation will be deleted by the surrounding drop_if
            }
            return match;
        }); //executes in order and drops every operation where predicate(op) == true

    if (!success) {
        //No OcppOperation was aborted because of the error message
        AO_DBG_WARN("Received CALLERROR did not abort a pending operation");
        (void)0;
    }
}

/*
 * Tries to recover the Ocpp-Operation header from a broken message.
 * 
 * Example input: 
 * [2, "75705e50-682d-404e-b400-1bca33d41e19", "ChangeConfiguration", {"key":"now the msg breaks...
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
