// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#include "OcppOperation.h"

#include "Variants.h"

int unique_id_counter = 531531531;


OcppOperation::OcppOperation(WebSocketsClient *webSocket, OcppMessage *ocppMessage) : webSocket(webSocket), ocppMessage(ocppMessage) {
  
}

OcppOperation::OcppOperation(WebSocketsClient *webSocket) : webSocket(webSocket) {
  ocppMessage = NULL;
}

OcppOperation::~OcppOperation(){
  if (ocppMessage != NULL)
    delete ocppMessage;
}

void OcppOperation::setOcppMessage(OcppMessage *msg){
  ocppMessage = msg;
}

void OcppOperation::setMessageID(String &id){
  if (messageID.length() > 0){
    Serial.print(F("[OcppOperation] OcppOperation (class): MessageID is set twice or is set after first usage!\n"));
    //return; <-- Just letting the id being overwritten probably doesn't cause further errors...
  }
  messageID = String(id);
}

String &OcppOperation::getMessageID() {
  if (messageID.isEmpty()) {
    messageID = String(unique_id_counter++);
  }
  return messageID;
}

boolean OcppOperation::sendReq(){

  /*
   * timeout behaviour
   * 
   * if timeout, print out error message and treat this operation as completed (-> return true)
   */
  if (timeout_active && millis() - timeout_start >= TIMEOUT_CANCEL){
    //cancel this operation
    Serial.print(F("[OcppOperation] "));
    Serial.print(ocppMessage->getOcppOperationType());
    Serial.print(F(" timeout! Cancel operation\n"));
    return true;
  }

  /*
   * retry behaviour
   * 
   * if retry, run the rest of this function, i.e. resend the message. If not, just return false
   * 
   * Mix timeout + retry?
   */
  if (millis() <= retry_start + RETRY_INTERVAL * retry_interval_mult) {
    //NO retry
    return false;
  }
  // Do retry. Increase timer by factor 2
  retry_interval_mult *= 2;

  /*
   * Create the OCPP message
   */
  DynamicJsonDocument *requestPayload = ocppMessage->createReq();

  /*
   * Create OCPP-J Remote Procedure Call header
   */
  size_t json_buffsize = JSON_ARRAY_SIZE(4) + (getMessageID().length() + 1) + requestPayload->capacity();
  DynamicJsonDocument requestJson(json_buffsize);

  requestJson.add(MESSAGE_TYPE_CALL);                    //MessageType
  requestJson.add(getMessageID());                       //Unique message ID
  requestJson.add(ocppMessage->getOcppOperationType());  //Action
  requestJson.add(*requestPayload);                      //Payload

  /*
   * Serialize and send. Destroy serialization and JSON object. 
   * 
   * If sending was successful, start timer
   * 
   * Return that this function must be called again (-> false)
   */
  String out;
  serializeJson(requestJson, out);
  if (DEBUG_OUT) Serial.print(F("[OcppOperation] Send requirement: "));
  if (DEBUG_OUT) Serial.print(out);
  if (DEBUG_OUT) Serial.print(F("\n"));
  if (webSocket->sendTXT(out)) {
    //success
    if (!timeout_active) {
      timeout_active = true;
      timeout_start = millis();
    } 
    retry_start = millis();
  } else {
    //webSocket is not able to put any data on TCP stack. Maybe because we're offline
    retry_start = 0;
    retry_interval_mult = 1;
  }

  requestJson.clear();
  delete requestPayload;
  return false;
}

boolean OcppOperation::receiveConf(JsonDocument *confJson){ //TODO add something like "JsonObject conf = confJson->as<JsonObject>();" and test
  /*
   * check if messageIDs match. If yes, continue with this function. If not, return false for message not consumed
   */
  if (!getMessageID().equals((*confJson)[1].as<String>())){
    return false;
  }

  /*
   * Hand the payload over to the OcppOperation object
   */
  JsonObject payload = (*confJson)[2];
  ocppMessage->processConf(payload);

  /*
   * Hand the payload over to the onReceiveConf Callback
   */
  if (onReceiveConfListener != NULL){
      onReceiveConfListener(payload);
      onReceiveConfListener = NULL; //just call once per instance
  }

  /*
   * return true as this message has been consumed
   */
  return true;
}


boolean OcppOperation::receiveReq(JsonDocument *reqJson){ //TODO add something like "JsonObject req = reqJson->as<JsonObject>();" and test
  
  String reqId = (*reqJson)[1];
  setMessageID(reqId);//(*reqJson)[1]);

  //TODO What if client receives requests two times? Can happen if previous conf is lost. In the Smart Charging Profile
  //     it probably doesn't matter to repeat an operation on the EVSE. Change in future?
  
  /*
   * Hand the payload over to the OcppOperation object
   */
  JsonObject payload = (*reqJson)[3];
  ocppMessage->processReq(payload);
  
  /*
   * Hand the payload over to the first Callback. It is a callback that notifies the client that request has been processed in the OCPP-library
   */

  if (onReceiveReqListener != NULL){
      onReceiveReqListener(payload);
      onReceiveReqListener = NULL; //just call once per instance
  }

  reqExecuted = true; //ensure that the conf is only sent after the req has been executed

  return true; //true because everything was successful. If there will be an error check in future, this value becomes more reasonable
}

boolean OcppOperation::sendConf(){

  if (!reqExecuted) {
    //wait until req has been executed
    return false;
  }

  /*
   * Create the OCPP message
   */
  DynamicJsonDocument *confPayload = ocppMessage->createConf();

  /*
   * Create OCPP-J Remote Procedure Call header
   */
  size_t json_buffsize = JSON_ARRAY_SIZE(3) + (getMessageID().length() + 1) + confPayload->capacity();
  DynamicJsonDocument confJson(json_buffsize);

  confJson.add(MESSAGE_TYPE_CALLRESULT);   //MessageType
  confJson.add(getMessageID());            //Unique message ID
  confJson.add(*confPayload);              //Payload  

  /*
   * Serialize and send. Destroy serialization and JSON object. 
   */
  String out;
  serializeJson(confJson, out);
  boolean success = webSocket->sendTXT(out);

  if (success) {
    if (DEBUG_OUT) Serial.print(F("[OcppOperation] (Conf) JSON message: "));
    if (DEBUG_OUT) Serial.print(out);
    if (DEBUG_OUT) Serial.print('\n');
    if (onSendConfListener != NULL){
      onSendConfListener(confPayload->as<JsonObject>());
      onSendConfListener = NULL; //just call once
    }
  }
  
  confJson.clear();
  delete confPayload;
  return success;
}

void OcppOperation::setOnReceiveConfListener(OnReceiveConfListener onReceiveConf){
  onReceiveConfListener = onReceiveConf;
}

/**
 * Sets a Listener that is called after this machine processed a request by the communication counterpart
 */
void OcppOperation::setOnReceiveReqListener(OnReceiveReqListener onReceiveReq){
  onReceiveReqListener = onReceiveReq;
}

void OcppOperation::setOnSendConfListener(OnSendConfListener onSendConf){
  onSendConfListener = onSendConf;
}

boolean OcppOperation::isFullyConfigured(){
  return ocppMessage != NULL;
}

void OcppOperation::print_debug() {
  Serial.print(F("[OcppOperation] I am a "));
  if (ocppMessage) {
    Serial.print(ocppMessage->getOcppOperationType());
  } else {
    Serial.print(F("NULL"));
  }
  Serial.print(F("\n"));
}
