#include "Variants.h"

#include "StatusNotification.h"

#include <string.h>

StatusNotification::StatusNotification(WebSocketsClient *webSocket, ChargePointStatus currentStatus) 
  : OcppOperation(webSocket)
  , currentStatus(currentStatus) {
 
  if (!getJsonDateStringFromSystemTime(timestamp, JSONDATE_LENGTH)){
    Serial.print(F("[StatusNotification] Error reading time string. Expect format like 2020-02-01T20:53:32.486Z\n"));
  }

  if (DEBUG_OUTPUT) {
    Serial.print(F("[StatusNotification] New StatusNotification with timestamp "));
    Serial.print(timestamp);
    Serial.print('\n');
  }

  switch (currentStatus) {
    case (ChargePointStatus::Available):
      if (DEBUG_OUTPUT) Serial.print(F("[StatusNotification] New Status: Available"));
      break;
    case (ChargePointStatus::Preparing):
      if (DEBUG_OUTPUT) Serial.print(F("[StatusNotification] New Status: Preparing"));
      break;
    case (ChargePointStatus::Charging):
      if (DEBUG_OUTPUT) Serial.print(F("[StatusNotification] New Status: Charging"));
      break;
    case (ChargePointStatus::SuspendedEVSE):
      if (DEBUG_OUTPUT) Serial.print(F("[StatusNotification] New Status: SuspendedEVSE"));
      break;
    case (ChargePointStatus::SuspendedEV):
      if (DEBUG_OUTPUT) Serial.print(F("[StatusNotification] New Status: SuspendedEV"));
      break;
    case (ChargePointStatus::Finishing):
      if (DEBUG_OUTPUT) Serial.print(F("[StatusNotification] New Status: Finishing"));
      break;
    case (ChargePointStatus::Reserved):
      if (DEBUG_OUTPUT) Serial.print(F("[StatusNotification] New Status: Reserved"));
      break;
    case (ChargePointStatus::Unavailable):
      if (DEBUG_OUTPUT) Serial.print(F("[StatusNotification] New Status: Unavailable"));
      break;
    case (ChargePointStatus::Faulted):
      if (DEBUG_OUTPUT) Serial.print(F("[StatusNotification] New Status: Faulted"));
      break;
    case (ChargePointStatus::NOT_SET):
      Serial.print(F("[StatusNotification] New Status: NOT_SET"));
      break;
    default:
      Serial.print(F("[StatusNotification] [Error, invalid status code]"));
      break;
  }
}

//TODO if the status has changed again when sendReq() is called, abort the operation completely (note: if req is already sent, stick with listening to conf). The ChargePointStatusService will enqueue a new operation itself
boolean StatusNotification::sendReq() {
  if (completed) {
    //Authorize request has been conf'd and is therefore completed
    return true;
  }
  if (waitForConf) {
    //Authorize request is already pending; nothing to do here
    return false;
  }

  /*
   * Generate JSON object
   */
  const size_t capacity = JSON_ARRAY_SIZE(4) + JSON_OBJECT_SIZE(4) + 100;
  DynamicJsonDocument doc(capacity);

  doc.add(MESSAGE_TYPE_CALL);              //MessageType
  doc.add(this->getMessageID());           //Unique message ID
  doc.add("StatusNotification");           //Action

  JsonObject doc_3 = doc.createNestedObject(); //Payload
  doc_3["connectorId"] = 1;        //Hardcoded to be one because only one connector is supported
  doc_3["errorCode"] = "NoError";  //No error diagnostics support
  
  switch (currentStatus) {
    case (ChargePointStatus::Available):
      doc_3["status"] = "Available";
      break;
    case (ChargePointStatus::Preparing):
      doc_3["status"] = "Preparing";
      break;
    case (ChargePointStatus::Charging):
      doc_3["status"] = "Charging";
      break;
    case (ChargePointStatus::SuspendedEVSE):
      doc_3["status"] = "SuspendedEVSE";
      break;
    case (ChargePointStatus::SuspendedEV):
      doc_3["status"] = "SuspendedEV";
      break;
    case (ChargePointStatus::Finishing):
      doc_3["status"] = "Finishing";
      break;
    case (ChargePointStatus::Reserved):
      doc_3["status"] = "Reserved";
      break;
    case (ChargePointStatus::Unavailable):
      doc_3["status"] = "Unavailable";
      break;
    case (ChargePointStatus::Faulted):
      doc_3["status"] = "Faulted";
      break;
    default:
      doc_3["status"] = "NOT_SET";
      Serial.print(F("[StatusNotification] Error: Sending status NOT_SET!\n"));
      break;
  }

  doc_3["timestamp"] = timestamp;

  char out[150];  
  size_t len = serializeJson(doc, out, 150);

  doc.clear();

  boolean success = webSocket->sendTXT(out, len);
  if (success) {
    if (DEBUG_APP_LAY) Serial.print(F("[StatusNotification] JSON message: "));
    if (DEBUG_APP_LAY) Serial.printf(out);
    if (DEBUG_APP_LAY) Serial.print('\n');
    waitForConf = true;
  }
  return false; //expecting a conf message (further processing on this OcppOperation) so return false
}


boolean StatusNotification::receiveConf(JsonDocument *json) {

  /**
   * Check if conf-message belongs to this operation instance
   */
  
  if ((*json)[1] != this->getMessageID()) {
    return false;
  }

  /**
   * Process the message
   */

  if (DEBUG_APP_LAY) Serial.print(F("[StatusNotification] Request has been accepted!\n"));
  completed = true;
  if (onCompleteListener != NULL){
    onCompleteListener(json);
    onCompleteListener = NULL; //just call once
  }
  waitForConf = false;

  return true; //Message has been consumed
}


/* ####################################
 * For debugging only: implement dummy server functionalities to test against echo server
 * #################################### */
StatusNotification::StatusNotification(WebSocketsClient *webSocket) 
  : OcppOperation(webSocket) {
    
}

/* ####################################
 * For debugging only: implement dummy server functionalities to test against echo server
 * #################################### */
boolean StatusNotification::receiveReq(JsonDocument *json) {

  this->setMessageID((*json)[1]);

  /**
   * Ignore Contents of this Req-message, because this is for debug purposes only
   */
   
  if (onReceiveReqListener != NULL){
      onReceiveReqListener(json);
      onReceiveReqListener = NULL; //just call once
  }

  reqExecuted = true;
  
  return true; //Message has been consumed
}

/* ####################################
 * For debugging only: implement dummy server functionalities to test against echo server
 * #################################### */
boolean StatusNotification::sendConf() {
  if (!reqExecuted) {
    //wait until req has been executed
    return false;
  }

  /*
   * Generate JSON object
   */
  const size_t capacity = JSON_ARRAY_SIZE(2);
  DynamicJsonDocument doc(capacity);

  doc.add(MESSAGE_TYPE_CALLRESULT);               //MessageType
  doc.add(this->getMessageID());       //Unique message ID

  char out[150];  
  size_t len = serializeJson(doc, out, 150);

  boolean success = webSocket->sendTXT(out, len);
  doc.clear();
  return success;
}
