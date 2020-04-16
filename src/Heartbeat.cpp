#include "Heartbeat.h"
#include "TimeHelper.h"
#include <string.h>


Heartbeat::Heartbeat(WebSocketsClient *webSocket) 
  : OcppOperation(webSocket) {
  
}

boolean Heartbeat::sendReq() {
  if (completed) {
    //Request has been conf'd and is therefore completed
    return true;
  }
  if (waitForConf) {
    //Request is already pending; nothing to do here
    return false;
  }
  
  if (DEBUG_APP_LAY) Serial.print(F("[Heartbeat] Request JSON: "));

  /*
   * Generate JSON object
   */
  const size_t capacity = JSON_ARRAY_SIZE(3) + JSON_OBJECT_SIZE(1) + 20;
  const int OUT_LEN = 100;
  DynamicJsonDocument doc(capacity);

  doc.add(MESSAGE_TYPE_CALL);               //MessageType
  doc.add(this->getMessageID());       //Unique message ID
  doc.add("Heartbeat");     //Action
  JsonObject doc_3 = doc.createNestedObject(); //empty payload

  char out[OUT_LEN];  
  size_t len = serializeJson(doc, out, OUT_LEN);

  //Serial.print(F("JSON message for delivery: "));
  if (DEBUG_APP_LAY) Serial.printf(out);
  if (DEBUG_APP_LAY) Serial.print('\n');

  doc.clear();

  boolean success = webSocket->sendTXT(out, len);
  if (success) {
    waitForConf = true; //package has been sent and waits for its answer
  }
  return false;
}

boolean Heartbeat::receiveConf(JsonDocument *json) {

  /**
   * Check if conf-message belongs to this operation instance
   */
  
  if ((*json)[1] != this->getMessageID()) {
    return false;
  }

  /**
   * Process the message
   */
  
  const char* currentTime = (*json)[2]["currentTime"] | "Invalid";
  if (strcmp(currentTime, "Invalid")) {
    if (setTimeFromJsonDateString(currentTime)) {
      if (DEBUG_APP_LAY) Serial.print(F("[Heartbeat] Request has been accepted!\n"));
    } else {
      Serial.print(F("[Heartbeat] Request accepted. But Error reading time string. Expect format like 2020-02-01T20:53:32.486Z\n"));
    }
  } else {
    Serial.print(F("[Heartbeat] Request accepted. But Error reading time string. Expect format like 2020-02-01T20:53:32.486Z\n"));
  }
  
  completed = true;
  if (onCompleteListener != NULL){
    onCompleteListener(json);
    onCompleteListener = NULL;
  }
  waitForConf = false;

  return true; //Message has been consumed
}

/* ####################################
 * For debugging only: implement dummy server functionalities to test against echo server
 * #################################### */
boolean Heartbeat::receiveReq(JsonDocument *json) {

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
boolean Heartbeat::sendConf() {
  if (!reqExecuted) {
    //wait until req has been executed
    return false;
  }

  /*
   * Generate JSON object
   */
  const size_t capacity = JSON_ARRAY_SIZE(3) + JSON_OBJECT_SIZE(1) + 100;
  const int OUT_LEN = 150;
  DynamicJsonDocument doc(capacity);

  doc.add(MESSAGE_TYPE_CALLRESULT);               //MessageType
  doc.add(this->getMessageID());       //Unique message ID
  JsonObject doc_2 = doc.createNestedObject(); //Payload

  char currentTime[JSONDATE_LENGTH + 1] = {'\0'};
  getJsonDateStringFromSystemTime(currentTime, JSONDATE_LENGTH);
  doc_2["currentTime"] = currentTime;

  char out[OUT_LEN];  
  size_t len = serializeJson(doc, out, OUT_LEN);

  boolean success = webSocket->sendTXT(out, len);
  doc.clear();
  return success;
}
