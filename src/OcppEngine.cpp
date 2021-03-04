// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include "OcppEngine.h"
#include "SimpleOcppOperationFactory.h"

#include "OcppError.h"


WebSocketsClient *wsocket;
SmartChargingService *ocppEngine_smartChargingService;
ChargePointStatusService *ocppEngine_chargePointStatusService;
MeteringService *ocppEngine_meteringService;

LinkedList<OcppOperation*> initiatedOcppOperations;
LinkedList<OcppOperation*> receivedOcppOperations;

int JSON_DOC_SIZE;

#define HEAP_GUARD 2000UL //will not accept JSON messages if it will result in less than HEAP_GUARD free bytes in heap

size_t removePayload(const char *src, size_t src_size, char *dst, size_t dst_size);

void ocppEngine_initialize(WebSocketsClient *ws, int DEFAULT_JSON_DOC_SIZE){
  wsocket = ws;
  JSON_DOC_SIZE = DEFAULT_JSON_DOC_SIZE; //TODO Find another approach where the Doc size is determined dynamically

  initiatedOcppOperations = LinkedList<OcppOperation*>();
  receivedOcppOperations = LinkedList<OcppOperation*>();
}

/**
 * Process raw application data coming from the websocket stream
 */
boolean processWebSocketEvent(const char* payload, size_t length){

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
              OcppOperation *op = makeOcppOperation(wsocket, new OutOfMemory(ESP.getFreeHeap(), length));
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
 * Send back error code
 */
boolean processWebSocketUnsupportedEvent(const char* payload, size_t length){
  bool deserializationSuccess = false;
  DynamicJsonDocument *doc = new DynamicJsonDocument(200);
  char onlyRpcHeader[200];
  size_t onlyRpcHeader_len = removePayload(payload, length, onlyRpcHeader, 200);
  DeserializationError err = deserializeJson(*doc, onlyRpcHeader, onlyRpcHeader_len);
  if (err.code() == DeserializationError::Ok) {
    int messageTypeId = (*doc)[0];
    if (messageTypeId == MESSAGE_TYPE_CALL) {
      deserializationSuccess = true;
      OcppOperation *op = makeOcppOperation(wsocket, new WebSocketError("This controller does not support WebSocket fragments"));
      handleReqMessage(doc, op);
    }
  } else {
    Serial.print(F("[OcppEngine] Deserialization of unsupported WebSocket event failed: "));
    Serial.println(err.c_str());
  }

  if (doc) delete doc;
  return deserializationSuccess;
}

void handleReqMessage(JsonDocument *json){
  OcppOperation *req = makeFromJson(wsocket, json);
  if (req == NULL) {
    Serial.print(F("[OcppEngine] Couldn't make OppOperation from Request. Ignore request.\n"));
    return;
  }
  handleReqMessage(json, req);
}

void handleReqMessage(JsonDocument *json, OcppOperation *req) {
  if (req == NULL) {
    Serial.print(F("[OcppEngine] handleReqMessage: invalid argument\n"));
    return;
  }
  receivedOcppOperations.add(req);; //enqueue so loop() plans conf sending
  req->receiveReq(json); //"fire" the operation
}

/**
   * call conf() on each element of the queue. Start with first element. On successful message delivery,
   * delete the element from the list. Try all the pending OCPP Operations until the right one is found.
   * 
   * This function could result in improper behavior in Charging Stations, because messages are not
   * guaranteed to be received and therefore processed in the right order.
   */
void handleConfMessage(JsonDocument *json){
  for (int i = 0; i < initiatedOcppOperations.size(); i++){
    OcppOperation *el = initiatedOcppOperations.get(i);
    boolean success = el->receiveConf(json); //maybe rename to "consumed"?
    if (success){
      initiatedOcppOperations.remove(i);
      
      //TODO Review: are all recources freed here?
      delete el;
      return;
    }
  }

  //didn't find matching OcppOperation
  Serial.print(F("[OcppEngine] Received CALLRESULT doesn't match any pending operation!\n"));
}


void handleErrMessage(JsonDocument *json){
  for (int i = 0; i < initiatedOcppOperations.size(); i++){
    OcppOperation *el = initiatedOcppOperations.get(i);
    boolean discardOperation = el->receiveError(json); //maybe rename to "consumed"?
    if (discardOperation){
      initiatedOcppOperations.remove(i);
      
      //TODO Review: are all recources freed here?
      delete el;
      return;
    }
  }

  //No OcppOperation was aborted because of the error message
  if (DEBUG_OUT) Serial.print(F("[OcppEngine] Received CALLERROR did not abort a pending operation\n"));
}


void initiateOcppOperation(OcppOperation *o){
  if (!o->isFullyConfigured()){
    Serial.printf("[OcppEngine] initiateOcppOperation(op) was called without the operation being configured and ready to send. Discard operation!\n");
    delete o;
    return;
  }
  initiatedOcppOperations.add(o);
}

void ocppEngine_loop(){

  /**
   * Work through the initiatedOcppOperations queue. Start with the first element by calling req() on it. If
   * the operation is (still) pending, it will return false and therefore each other queued element must
   * wait until this operation is over. If an ocppOperation is finished, it returns true on a req() call, 
   * can be dequeued and the following ocppOperation is processed.
   */
  
  while (initiatedOcppOperations.size() > 0){
    OcppOperation *el = initiatedOcppOperations.get(0);
    boolean timeout = el->sendReq(); //The only reason to dequeue elements here is when a timeout occurs. Normally
    if (timeout){                    //the Conf msg processing routine dequeues finished elements
      initiatedOcppOperations.remove(0);
      
      //TODO Review: are all recources freed here?
      delete el;
      //go on with the next element in the queue, which is now at initiatedOcppOperations[0]
    } else {
      //there is one operation pending right now, so quit this while-loop.
      break;
    }
  }

  /*
   * Activate timeout detection on the msgs other than the first in the queue.
   */

  for (int i = 1; i < initiatedOcppOperations.size(); i++) {
    Timeout *timer = initiatedOcppOperations.get(i)->getTimeout();
    if (timer)
      timer->tick(false); //false: did not send a frame prior to calling tick
      if (timer->isExceeded()) {
        Serial.print(F("[OcppEngine] Discarding operation due to timeout: "));
        initiatedOcppOperations.get(i)->print_debug();
        initiatedOcppOperations.remove(i);
        delete initiatedOcppOperations.get(i);
      }
  }

  /**
   * Work through the receivedOcppOperations queue. Start with the first element by calling conf() on it. 
   * If an ocppOperation is finished, it returns true on a conf() call, and is dequeued.
   */
  
  int i = 0;
  while (i < receivedOcppOperations.size()){
    OcppOperation *el = receivedOcppOperations.get(i);
    boolean success = el->sendConf();
    if (success){
      receivedOcppOperations.remove(i);

      //TODO Review: are all recources freed here?
      delete el;
      //go on with the next element in the queue, which is now at receivedOcppOperations[i]
    } else {
      //There will be another attempt to send this conf message in a future loop call.
      //Go on with the next element in the queue, which is now at receivedOcppOperations[i+1]
      i++;
    }
  }

  
}

void setSmartChargingService(SmartChargingService *scs) {
  ocppEngine_smartChargingService = scs;
}

SmartChargingService* getSmartChargingService(){
  if (ocppEngine_smartChargingService == NULL) {
    Serial.print(F("[OcppEngine] Error: in OcppEngine, there is no smartChargingService set, but it is accessed!\n"));
    //no error catch 
  }
  return ocppEngine_smartChargingService;
}

void setChargePointStatusService(ChargePointStatusService *cpss){
  ocppEngine_chargePointStatusService = cpss;
}

ChargePointStatusService *getChargePointStatusService(){
  if (ocppEngine_chargePointStatusService == NULL) {
    Serial.print(F("[OcppEngine] Error: in OcppEngine, there is no chargePointStatusService set, but it is accessed!\n"));
    //no error catch 
  }
  return ocppEngine_chargePointStatusService;
}

ConnectorStatus *getConnectorStatus(int connectorId) {
  if (getChargePointStatusService() == NULL) return NULL;

  ConnectorStatus *result = getChargePointStatusService()->getConnector(connectorId);
  if (result == NULL) {
    Serial.print(F("[OcppEngine] Error in getConnectorStatus(): cannot fetch connector with given connectorId!\n"));
    //no error catch 
  }
  return result;
}

void setMeteringSerivce(MeteringService *meteringService) {
ocppEngine_meteringService = meteringService;
}

MeteringService* getMeteringService() {
  if (ocppEngine_meteringService == NULL) {
    Serial.print(F("[OcppEngine] Error: in OcppEngine, there is no ocppEngine_meteringService set, but it is accessed!\n"));
    //no error catch 
  }
  return ocppEngine_meteringService;
}

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
