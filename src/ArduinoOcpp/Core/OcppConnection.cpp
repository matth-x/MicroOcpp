// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/Core/OcppConnection.h>

#include <ArduinoOcpp/SimpleOcppOperationFactory.h>
#include <ArduinoOcpp/Core/OcppError.h>


#define HEAP_GUARD 2000UL //will not accept JSON messages if it will result in less than HEAP_GUARD free bytes in heap

size_t removePayload(const char *src, size_t src_size, char *dst, size_t dst_size);

using namespace ArduinoOcpp;

OcppConnection::OcppConnection(OcppSocket *ocppSock) : ocppSock(ocppSock) {
    ReceiveTXTcallback callback = [this] (const char *payload, size_t length) {
        return this->processOcppSocketInputTXT(payload, length);
    };
    
    ocppSock->setReceiveTXTcallback(callback);
}

void OcppConnection::loop() {
    /**
      * Work through the initiatedOcppOperations queue. Start with the first element by calling req() on it. If
      * the operation is (still) pending, it will return false and therefore each other queued element must
      * wait until this operation is over. If an ocppOperation is finished, it returns true on a req() call, 
      * can be dequeued and the following ocppOperation is processed.
      */

    auto operation = initiatedOcppOperations.begin();
    while (operation != initiatedOcppOperations.end()){
        boolean timeout = (*operation)->sendReq(ocppSock); //The only reason to dequeue elements here is when a timeout occurs. Normally
        if (timeout){                                      //the Conf msg processing routine dequeues finished elements
            delete *operation;
            operation = initiatedOcppOperations.erase(operation);
            
            //TODO Review: are all recources freed here?
            //go on with the next element in the queue, which is now at initiatedOcppOperations[0]
        } else {
            //there is one operation pending right now, so quit this while-loop.
            break;
        }
    }

    /*
     * Activate timeout detection on the msgs other than the first in the queue.
     */

    operation = initiatedOcppOperations.begin();
    while (operation != initiatedOcppOperations.end()) {
        if (operation == initiatedOcppOperations.begin()){ //jump over the first element
            ++operation; 
            continue; 
        } 
        Timeout *timer = (*operation)->getTimeout();
        if (!timer) {
            ++operation; //no timeouts, nothing to do in this iteration
            continue;
        }
        timer->tick(false); //false: did not send a frame prior to calling tick
        if (timer->isExceeded()) {
            Serial.print(F("[OcppEngine] Discarding operation due to timeout: "));
            (*operation)->print_debug();
            delete *operation;
            operation = initiatedOcppOperations.erase(operation);
        } else {
            ++operation;
        }
    }
    
    /**
     * Work through the receivedOcppOperations queue. Start with the first element by calling conf() on it. 
     * If an ocppOperation is finished, it returns true on a conf() call, and is dequeued.
     */

    operation = receivedOcppOperations.begin();
    while (operation != receivedOcppOperations.end()){
        boolean success = (*operation)->sendConf(ocppSock);
        if (success){
            delete *operation;
            operation = receivedOcppOperations.erase(operation);

            //TODO Review: are all recources freed here?
            //go on with the next element in the queue, which is now at receivedOcppOperations[i]
        } else {
            //There will be another attempt to send this conf message in a future loop call.
            //Go on with the next element in the queue, which is now at receivedOcppOperations[i+1]
            ++operation; //TODO review: this makes conf's out-of-order. But if the first Op fails because of lacking RAM, this could save the device. 
        }
    }
}

void OcppConnection::initiateOcppOperation(OcppOperation *o){
  if (!o->isFullyConfigured()){
    Serial.printf("[OcppEngine] initiateOcppOperation(op) was called without the operation being configured and ready to send. Discard operation!\n");
    delete o;
    return;
  }
  initiatedOcppOperations.push_back(o);
  o->setInitiated();
}

bool OcppConnection::processOcppSocketInputTXT(const char* payload, size_t length) {
    
  boolean deserializationSuccess = false;

  DynamicJsonDocument *doc = NULL;
  size_t capacity = length + 100;

  DeserializationError err = DeserializationError::NoMemory;
  while (capacity + HEAP_GUARD < ESP.getFreeHeap() && err == DeserializationError::NoMemory) {
      if (doc){
          delete doc;
          doc = NULL;
      }
      doc = new DynamicJsonDocument(capacity);
      err = deserializeJson(*doc, payload, length);

      capacity /= 2;
      capacity *= 3;
  }

  //TODO insert validateRpcHeader at suitable position

  switch (err.code()) {
    case DeserializationError::Ok:
        if (doc != NULL){
          int messageTypeId = (*doc)[0];
          switch(messageTypeId) {
            case MESSAGE_TYPE_CALL:
              handleReqMessage(doc);      
              deserializationSuccess = true;
              break;
            case MESSAGE_TYPE_CALLRESULT:
              handleConfMessage(doc);
              deserializationSuccess = true;
              break;
            case MESSAGE_TYPE_CALLERROR:
              handleErrMessage(doc);
              deserializationSuccess = true;
              break;
            default:
              Serial.print(F("[OcppEngine] Invalid OCPP message! (though JSON has successfully been deserialized)\n"));
              break;
          }
        } else { //unlikely corner case
            Serial.print(F("[OcppEngine] Deserialization is okay but doc is Null\n"));
        }
        break;
    case DeserializationError::InvalidInput:
        Serial.print(F("[OcppEngine] Invalid input! Not a JSON\n"));
        break;
    case DeserializationError::NoMemory:
        {
          if (DEBUG_OUT) Serial.print(F("[OcppEngine] Error: Not enough memory in heap! Input length = "));
          if (DEBUG_OUT) Serial.print(length);
          if (DEBUG_OUT) Serial.print(F(", free heap = "));
          if (DEBUG_OUT) Serial.println(ESP.getFreeHeap());

          /*
          * If websocket input is of message type MESSAGE_TYPE_CALL, send back a message of type MESSAGE_TYPE_CALLERROR.
          * Then the communication counterpart knows that this operation failed.
          * If the input type is MESSAGE_TYPE_CALLRESULT, it can be ignored. This controller will automatically resend the corresponding requirement message.
          */

          if (doc){
              delete doc;
              doc = NULL;
          }

          doc = new DynamicJsonDocument(200);
          char onlyRpcHeader[200];
          size_t onlyRpcHeader_len = removePayload(payload, length, onlyRpcHeader, 200);
          DeserializationError err2 = deserializeJson(*doc, onlyRpcHeader, onlyRpcHeader_len);
          if (err2.code() == DeserializationError::Ok) {
            int messageTypeId2 = (*doc)[0] | -1;
            if (messageTypeId2 == MESSAGE_TYPE_CALL) {
              deserializationSuccess = true;
              OcppOperation *op = makeOcppOperation(new OutOfMemory(ESP.getFreeHeap(), length));
              handleReqMessage(doc, op);
            }
          }
        }
        break;
    default:
        Serial.print(F("[OcppEngine] Deserialization failed: "));
        Serial.println(err.c_str());
        break;
  }

  if (doc) {
    delete doc;
  }

  return deserializationSuccess;
}

/**
   * call conf() on each element of the queue. Start with first element. On successful message delivery,
   * delete the element from the list. Try all the pending OCPP Operations until the right one is found.
   * 
   * This function could result in improper behavior in Charging Stations, because messages are not
   * guaranteed to be received and therefore processed in the right order.
   */
void OcppConnection::handleConfMessage(JsonDocument *json) {
    for (auto operation = initiatedOcppOperations.begin(); operation != initiatedOcppOperations.end(); ++operation) {
        boolean success = (*operation)->receiveConf(json); //maybe rename to "consumed"?
        if (success) {
            delete *operation;
            initiatedOcppOperations.erase(operation);
            return;
        }
    }

  //didn't find matching OcppOperation
  Serial.print(F("[OcppEngine] Received CALLRESULT doesn't match any pending operation!\n"));
}

void OcppConnection::handleReqMessage(JsonDocument *json) {
    OcppOperation *req = makeFromJson(json);
    if (req == NULL) {
        Serial.print(F("[OcppEngine] Couldn't make OppOperation from Request. Ignore request.\n"));
        return;
    }
    handleReqMessage(json, req);
}

void OcppConnection::handleReqMessage(JsonDocument *json, OcppOperation *op) {
    if (op == NULL) {
        Serial.print(F("[OcppEngine] handleReqMessage: invalid argument\n"));
        return;
    }
    receivedOcppOperations.push_back(op);; //enqueue so loop() plans conf sending
    op->receiveReq(json); //"fire" the operation
}

void OcppConnection::handleErrMessage(JsonDocument *json) {
    for (auto operation = initiatedOcppOperations.begin(); operation != initiatedOcppOperations.end(); ++operation){
        boolean discardOperation = (*operation)->receiveError(json); //maybe rename to "consumed"?
        if (discardOperation){
            //TODO Review: are all recources freed here?
            delete *operation;
            initiatedOcppOperations.erase(operation);
            return;
        }
    }

    //No OcppOperation was aborted because of the error message
    if (DEBUG_OUT) Serial.print(F("[OcppEngine] Received CALLERROR did not abort a pending operation\n"));
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

