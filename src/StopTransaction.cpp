#include "Variants.h"

#include "StopTransaction.h"
#include "OcppEngine.h"
#include "MeteringService.h"


StopTransaction::StopTransaction(WebSocketsClient *webSocket) 
  : OcppOperation(webSocket) {
  
}

boolean StopTransaction::sendReq() {
  if (completed) {
    //StartTransaction request has been conf'd and is therefore completed
    return true;
  }
  if (waitForConf) {
    //StartTransaction request is already pending; nothing to do here
    return false;
  }

  /*
   * Generate JSON object
   */
  const size_t capacity = JSON_ARRAY_SIZE(4) + JSON_OBJECT_SIZE(4) + 50;
  const int OUT_LEN = 180;

  //const size_t capacity = JSON_ARRAY_SIZE(4) + JSON_OBJECT_SIZE(4) + 50;
  DynamicJsonDocument doc(capacity);

  doc.add(MESSAGE_TYPE_CALL);      //MessageType
  doc.add(this->getMessageID());   //Unique message ID
  doc.add("StopTransaction");            //Action
  JsonObject doc_3 = doc.createNestedObject(); //Payload
  doc_3["idTag"] = "fefed1d19876";
  float meterStop = 0.0f;
  if (getMeteringService() != NULL) {
    meterStop = getMeteringService()->readEnergyActiveImportRegister();
  }
  doc_3["meterStop"] = meterStop; //TODO meterStart is required to be in Wh, but measuring unit is probably inconsistent in implementation
  char timestamp[JSONDATE_LENGTH + 1] = {'\0'};
  getJsonDateStringFromSystemTime(timestamp, JSONDATE_LENGTH);
  doc_3["timestamp"] = timestamp;

  int transactionId = -1;
  if (getChargePointStatusService() != NULL) {
    transactionId = getChargePointStatusService()->getTransactionId();
  }
  doc_3["transactionId"] = transactionId;

  char out[OUT_LEN];  
  size_t len = serializeJson(doc, out, OUT_LEN);

  if (DEBUG_APP_LAY) Serial.print(F("[StopTransaction] JSON message: "));
  if (DEBUG_APP_LAY) Serial.printf(out);
  if (DEBUG_APP_LAY) Serial.print('\n');

  //TODO destroy JSON doc
  doc.clear();

  boolean success = webSocket->sendTXT(out, len);
  if (success) {
    waitForConf = true; //package has been sent and waits for its answer
  }
  return false;
}

boolean StopTransaction::receiveConf(JsonDocument *json) {

  /**
   * Check if conf-message belongs to this operation instance
   */
  
  if ((*json)[1] != this->getMessageID()) {
    return false;
  }

  /**
   * Process the message
   */

  //no need to process anything here

  ChargePointStatusService *cpStatusService = getChargePointStatusService();
  if (cpStatusService != NULL){
    cpStatusService->stopTransaction();
    cpStatusService->unauthorize();
  }

  SmartChargingService *scService = getSmartChargingService();
    if (scService != NULL){
      scService->endChargingNow();
    }

  if (DEBUG_APP_LAY) Serial.print(F("[StopTransaction] Request has been accepted!\n"));
  
  completed = true;
  if (onCompleteListener != NULL){
    onCompleteListener(json);
    onCompleteListener = NULL; //just call once per instance
  }
  waitForConf = false;

  return true; //Message has been consumed
}


/* ####################################
 * For debugging only: implement dummy server functionalities to test against echo server
 * #################################### */
boolean StopTransaction::receiveReq(JsonDocument *json) {

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
boolean StopTransaction::sendConf() {
  if (!reqExecuted) {
    //wait until req has been executed
    return false;
  }

  /*
   * Generate JSON object
   */
  const size_t capacity = JSON_ARRAY_SIZE(3) + 2 * JSON_OBJECT_SIZE(1);
  DynamicJsonDocument doc(capacity);

  doc.add(MESSAGE_TYPE_CALLRESULT);               //MessageType
  doc.add(this->getMessageID());       //Unique message ID
  JsonObject doc_2 = doc.createNestedObject(); //Payload
  JsonObject idTagInfo = doc_2.createNestedObject("idTagInfo");
  idTagInfo["status"] = "Accepted";

  char out[150];  
  size_t len = serializeJson(doc, out, 150);

  boolean success = webSocket->sendTXT(out, len);
  doc.clear();
  return success;
}
