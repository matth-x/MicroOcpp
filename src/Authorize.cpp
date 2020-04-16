#include "Variants.h"

#include "Authorize.h"
#include "OcppEngine.h"


Authorize::Authorize(WebSocketsClient *webSocket) 
  : OcppOperation(webSocket) {
  
}

boolean Authorize::sendReq() {
  if (completed) {
    //Authorize request has been conf'd and is therefore completed
    return true;
  }
  if (waitForConf) {
    //Authorize request is already pending; nothing to do here
    return false;
  }

  if (DEBUG_APP_LAY) Serial.print(F("[Authorize] Request JSON: "));

  /*
   * Generate JSON object
   */

  const size_t capacity = JSON_ARRAY_SIZE(4) + JSON_OBJECT_SIZE(1) + 20;
  const int OUT_LEN = 100;

  DynamicJsonDocument doc(capacity);

  doc.add(MESSAGE_TYPE_CALL);      //MessageType
  doc.add(this->getMessageID());   //Unique message ID
  doc.add("Authorize");            //Action
  JsonObject doc_3 = doc.createNestedObject(); //Payload
  doc_3["idTag"] = "fefed1d19876";

  char out[OUT_LEN];  
  size_t len = serializeJson(doc, out, OUT_LEN);

  if (DEBUG_APP_LAY) Serial.printf(out);
  if (DEBUG_APP_LAY) Serial.print('\n');

  doc.clear();

  boolean success = webSocket->sendTXT(out, len);
  if (success) {
    waitForConf = true; //package has been sent and waits for its answer
  }
  
  return false;
}

boolean Authorize::receiveConf(JsonDocument *json) {
  /**
   * Check if conf-message belongs to this operation instance
   */
  
  if ((*json)[1] != this->getMessageID()) {
    return false;
  }

  /**
   * Process the message
   */

  const char* idTagInfo = (*json)[2]["idTagInfo"]["status"] | "Invalid";

  if (!strcmp(idTagInfo, "Accepted")) {
    if (DEBUG_APP_LAY) Serial.print(F("[Authorize] Request has been accepted!\n"));

    ChargePointStatusService *cpStatusService = getChargePointStatusService();
    if (cpStatusService != NULL){
      cpStatusService->authorize();
    }
    
    completed = true;
    if (onCompleteListener != NULL){
      onCompleteListener(json);
      onCompleteListener = NULL; //just call once per instance
    }
    waitForConf = false;
  } else {
    Serial.print(F("[Authorize] Request has been denied!"));
  }

  return true; //Message has been consumed
}

/* ####################################
 * For debugging only: implement dummy server functionalities to test against echo server
 * #################################### */
boolean Authorize::receiveReq(JsonDocument *json) {

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
boolean Authorize::sendConf() {
  if (!reqExecuted) {
    //wait until req has been executed
    return false;
  }

  /*
   * Generate JSON object
   */
  const size_t capacity = JSON_ARRAY_SIZE(3) + 2 * JSON_OBJECT_SIZE(1) + 50;
  const int OUT_LEN = 150;
  DynamicJsonDocument doc(capacity);

  doc.add(MESSAGE_TYPE_CALLRESULT);               //MessageType
  doc.add(this->getMessageID());       //Unique message ID
  JsonObject doc_2 = doc.createNestedObject(); //Payload
  JsonObject idTagInfo = doc_2.createNestedObject("idTagInfo");
  idTagInfo["status"] = "Accepted";

  char out[OUT_LEN];  
  size_t len = serializeJson(doc, out, OUT_LEN);

  boolean success = webSocket->sendTXT(out, len);
  doc.clear();
  return success;
}
