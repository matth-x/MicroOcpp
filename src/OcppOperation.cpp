#include "OcppOperation.h"

int unique_message_id_counter = 424242;

OcppOperation::OcppOperation(WebSocketsClient *webSocket) : webSocket(webSocket){
  
}

OcppOperation::~OcppOperation(){
  
}

void OcppOperation::setMessageID(int id){
  if (messageID >= 0){
    Serial.print(F("[OcppOperation] OcppOperation (class): MessageID is set twice or is set after first usage!\n"));
    //return; <-- Just letting the id being overwritten probably doesn't cause further errors...
  }
  messageID = id;
}

int OcppOperation::getMessageID() {
  if (messageID < 0) {
    //TODO make this counter always greater than the last received number from the Ocpp connection counterpart <-- really necessary? Don't think anymore. Check
    messageID = unique_message_id_counter++;
  }
  return messageID;
}

boolean OcppOperation::sendReq(){
  Serial.print(F("[OcppOperation] Unsupported operation exception: sendReq() is not implemented!"));
  return true;
}

boolean OcppOperation::receiveConf(JsonDocument *json){
  Serial.print(F("[OcppOperation] Unsupported operation exception: receiveConf() is not implemented!"));
  return false;
}

boolean OcppOperation::receiveReq(JsonDocument *json){
  Serial.print(F("[OcppOperation] Unsupported operation exception: receiveReq() is not implemented!"));
  return false;
}

boolean OcppOperation::sendConf(){
  Serial.print(F("[OcppOperation] Unsupported operation exception: sendConf() is not implemented!"));
  return true;
}

void OcppOperation::setOnCompleteListener(OnCompleteListener onComplete){
  onCompleteListener = onComplete;
}

void OcppOperation::setOnReceiveReq(OnReceiveReqListener onReceiveReq){
  onReceiveReqListener = onReceiveReq;
}
