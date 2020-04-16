#include "Variants.h"

#include "BootNotification.h"
#include "EVSE.h"
#include "OcppEngine.h"

#include <string.h>
#include "TimeHelper.h"

BootNotification::BootNotification(WebSocketsClient *webSocket) 
  : OcppOperation(webSocket) {

}

boolean BootNotification::sendReq() {
  if (completed) {
    //Authorize request has been conf'd and is therefore completed
    return true;
  }
  if (waitForConf) {
    //Authorize request is already pending; nothing to do here
    return false;
  }

  if (DEBUG_APP_LAY) Serial.print(F("[BootNotification] Request JSON: "));

  /*
   * Generate JSON object
   */
  const size_t capacity = JSON_ARRAY_SIZE(4) + JSON_OBJECT_SIZE(3) + 50;
  const int OUT_LEN = 150;
  DynamicJsonDocument doc(capacity);

  doc.add(MESSAGE_TYPE_CALL);               //MessageType
  doc.add(this->getMessageID());       //Unique message ID
  doc.add("BootNotification");     //Action

  JsonObject doc_3 = doc.createNestedObject(); //Payload

  doc_3["chargePointVendor"] = EVSE_getChargePointVendor();
  doc_3["chargePointSerialNumber"] = EVSE_getChargePointSerialNumber();
  doc_3["chargePointModel"] = EVSE_getChargePointModel();

  char out[OUT_LEN];  
  size_t len = serializeJson(doc, out, OUT_LEN);
  
  if (DEBUG_APP_LAY) Serial.printf(out);
  if (DEBUG_APP_LAY) Serial.print('\n');

  doc.clear();

  boolean success = webSocket->sendTXT(out, len);
  if (success) {
    waitForConf = true;
  }

  return false;
}


boolean BootNotification::receiveConf(JsonDocument *json) {
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
    setTimeFromJsonDateString(currentTime);
  } else {
    Serial.print(F("[BootNotification] Error reading time string. Expect format like 2020-02-01T20:53:32.486Z\n"));
  }
  
  int interval = (*json)[2]["interval"];
  //TODO Review: completely ignore heartbeatinterval? Heartbeat isn't necessary for JSON implementations
  //(See OCPP-J 1.6 documentation for more details)

  const char* status = (*json)[2]["status"];

  if (!strcmp(status, "Accepted")) {
    if (DEBUG_APP_LAY) Serial.print(F("[BootNotification] Request has been accepted!\n"));
    if (getChargePointStatusService() != NULL) {
      getChargePointStatusService()->boot();
    }
    completed = true;
    if (onCompleteListener != NULL){
      onCompleteListener(json);
      onCompleteListener = NULL; //just call once
    }
    waitForConf = false;
  } else {
    Serial.print(F("[BootNotification] Request unsuccessful!\n"));
  }
  //TODO: catch status "Pending"

  return true; //Message has been consumed
}


/* ####################################
 * For debugging only: implement dummy server functionalities to test against echo server
 * #################################### */
boolean BootNotification::receiveReq(JsonDocument *json) {

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
boolean BootNotification::sendConf() {
  if (!reqExecuted) {
    //wait until req has been executed
    return false;
  }

  /*
   * Generate JSON object
   */
  const size_t capacity = JSON_ARRAY_SIZE(3) + JSON_OBJECT_SIZE(3) + 100;
  DynamicJsonDocument doc(capacity);

  doc.add(MESSAGE_TYPE_CALLRESULT);               //MessageType
  doc.add(this->getMessageID());       //Unique message ID
  JsonObject doc_2 = doc.createNestedObject(); //Payload

  char currentTime[JSONDATE_LENGTH + 1] = {'\0'};
  getJsonDateStringFromSystemTime(currentTime, JSONDATE_LENGTH);
  doc_2["currentTime"] = currentTime;
  doc_2["interval"] = 86400; //heartbeat send interval - not relevant for JSON variant of OCPP so send dummy value that likely won't break
  doc_2["status"] = "Accepted";

  char out[150];  
  size_t len = serializeJson(doc, out, 150);

  boolean success = webSocket->sendTXT(out, len);
  doc.clear();
  return success;
}
