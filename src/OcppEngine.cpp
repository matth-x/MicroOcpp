// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#include "OcppEngine.h"
#include "SimpleOcppOperationFactory.h"


WebSocketsClient *wsocket;
SmartChargingService *ocppEngine_smartChargingService;
ChargePointStatusService *ocppEngine_chargePointStatusService;
MeteringService *ocppEngine_meteringService;

LinkedList<OcppOperation*> initiatedOcppOperations;
LinkedList<OcppOperation*> receivedOcppOperations;

int JSON_DOC_SIZE;

void ocppEngine_initialize(WebSocketsClient *ws, int DEFAULT_JSON_DOC_SIZE){
  wsocket = ws;
  JSON_DOC_SIZE = DEFAULT_JSON_DOC_SIZE; //TODO Find another approach where the Doc size is determined dynamically

  initiatedOcppOperations = LinkedList<OcppOperation*>();
  receivedOcppOperations = LinkedList<OcppOperation*>();
}

/**
 * Process raw application data coming from the websocket stream
 */
boolean processWebSocketEvent(uint8_t * payload, size_t length){

  boolean success = false;

  DynamicJsonDocument doc(JSON_DOC_SIZE); //TODO Check: Can default JSON_DOC_SIZE just be replaced by param length?
  DeserializationError err = deserializeJson(doc, payload);
  switch (err.code()) {
    case DeserializationError::Ok:
        success = true;
        break;
    case DeserializationError::InvalidInput:
        Serial.print(F("[OcppEngine] Invalid input! Not a JSON\n"));
        break;
    case DeserializationError::NoMemory:
        Serial.print(F("[OcppEngine] Error: Not enough memory\n"));
        break;
    default:
        Serial.print(F("[OcppEngine] Deserialization failed\n"));
        break;
  }

  if (!success) {
    //propably the payload just wasn't a JSON message
    doc.clear();
    return false;
  }
  
  int messageTypeId = doc[0];
  switch(messageTypeId) {
    case MESSAGE_TYPE_CALL:
      handleReqMessage(&doc);      
      success = true;
      break;
    case MESSAGE_TYPE_CALLRESULT:
      handleConfMessage(&doc);
      success = true;
      break;
    case MESSAGE_TYPE_CALLERROR:
      handleErrMessage(&doc);
      success = true;
      break;
    default:
      Serial.print(F("[OcppEngine] Invalid OCPP message! (though JSON has successfully been deserialized)\n"));
      break;
  }

  doc.clear();
  return success;
}


void handleReqMessage(JsonDocument *json){
  OcppOperation *req = makeFromJson(wsocket, json);
  if (req == NULL) {
    Serial.print(F("[OcppEngine] Couldn't make OppOperation from Request. Ignore request.\n"));
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
  Serial.printf("[OcppEngine] Received CALLERROR\n"); //delete when implemented
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
