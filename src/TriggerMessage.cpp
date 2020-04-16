#include "Variants.h"

#include "TriggerMessage.h"
#include "SimpleOcppOperationFactory.h"
#include "OcppEngine.h"

TriggerMessage::TriggerMessage(WebSocketsClient *webSocket) 
  : OcppOperation(webSocket) {
  
}

boolean TriggerMessage::receiveReq(JsonDocument *json) {

  this->setMessageID((*json)[1]);

  /**
   * Process the message
   */

  //(*json)[0] is MessageType
  //(*json)[1] is MessageID
  //(*json)[2] is Action
  //(*json)[3] is Payload

  triggeredOperation = makeFromTriggerMessage(webSocket, json);
  if (triggeredOperation != NULL) {
    statusMessage = "Accepted";
  } else {
    Serial.print(F("[TriggerMessage] Couldn't make OppOperation from TriggerMessage. Ignore request.\n"));
    statusMessage = "NotImplemented";
  }
  //After sending conf for TriggerMessage.req that will happen
  //  initiateOcppOperation(triggeredOperation);

  if (onReceiveReqListener != NULL){
      onReceiveReqListener(json);
      onReceiveReqListener = NULL; //just call once
  }
  
  reqExecuted = true;
  
  return true; //Message has been consumed
}

boolean TriggerMessage::sendConf() {
  if (!reqExecuted) {
    //wait until req has been executed
    return false;
  }

  /*
   * Generate JSON object
   */
  const size_t capacity = JSON_ARRAY_SIZE(4) + JSON_OBJECT_SIZE(2);
  const int OUT_LEN = 150;

  DynamicJsonDocument doc(capacity);

  doc.add(MESSAGE_TYPE_CALLRESULT);               //MessageType
  doc.add(this->getMessageID());       //Unique message ID
  JsonObject doc_2 = doc.createNestedObject(); //Payload
  doc_2["status"] = statusMessage;

  char out[OUT_LEN];  
  size_t len = serializeJson(doc, out, OUT_LEN);

  boolean success = webSocket->sendTXT(out, len);
  if (success) {
    if (DEBUG_APP_LAY) Serial.print(F("[TriggerMessage] (conf) JSON message: "));
    if (DEBUG_APP_LAY) Serial.printf(out);
    if (DEBUG_APP_LAY) Serial.print('\n');

    /*
     * When TriggerMessage.conf has successfully been sent, initiate the triggered OCPP Operation
     */
    if (triggeredOperation != NULL) {
      initiateOcppOperation(triggeredOperation);
      triggeredOperation = NULL;
    }
    
    if (onCompleteListener != NULL){
      onCompleteListener(&doc);
      onCompleteListener = NULL; //just call once
    }
  }
  doc.clear();
  return success;
}
