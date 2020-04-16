#include "Variants.h"

#include "StartTransaction.h"
#include "TimeHelper.h"
#include "OcppEngine.h"
#include "MeteringService.h"


StartTransaction::StartTransaction(WebSocketsClient *webSocket) 
  : OcppOperation(webSocket) {
  
}

boolean StartTransaction::sendReq() {
  if (completed) {
    //StartTransaction request has been conf'd and is therefore completed
    return true;
  }
  if (waitForConf) {
    //StartTransaction request is already pending; nothing to do here
    return false;
  }
  
  //Serial.print(F("StartTransaction request with dummy ID: fefed1d19876.\n"));

  /*
   * Generate JSON object
   */
  const size_t capacity = JSON_ARRAY_SIZE(4) + JSON_OBJECT_SIZE(5) + 60;
  const int OUT_LEN = 180;

  //const size_t capacity = JSON_ARRAY_SIZE(4) + JSON_OBJECT_SIZE(1) + 20;
  DynamicJsonDocument doc(capacity);

  doc.add(MESSAGE_TYPE_CALL);      //MessageType
  doc.add(this->getMessageID());   //Unique message ID
  doc.add("StartTransaction");            //Action
  JsonObject doc_3 = doc.createNestedObject(); //Payload

  doc_3["connectorId"] = 1;
  MeteringService* meteringService = getMeteringService();
  if (meteringService != NULL) {
	doc_3["meterStart"] = meteringService->readEnergyActiveImportRegister();
  }
  char timestamp[JSONDATE_LENGTH + 1] = {'\0'};
  getJsonDateStringFromGivenTime(timestamp, JSONDATE_LENGTH + 1, now());
  doc_3["timestamp"] = timestamp;
  doc_3["idTag"] = "fefed1d19876";

  char out[OUT_LEN];  
  size_t len = serializeJson(doc, out, OUT_LEN);

  if (DEBUG_APP_LAY) Serial.print(F("[StartTransaction] JSON message: "));
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

boolean StartTransaction::receiveConf(JsonDocument *json) {

  /**
   * Check if conf-message belongs to this operation instance
   */
  
  if ((*json)[1] != this->getMessageID()) {
    return false;
  }

  /**
   * Process the message
   */

  const char* idTagInfoStatus = (*json)[2]["idTagInfo"]["status"] | "Invalid";
  int transactionId = (*json)[2]["transactionId"] | -1; //TODO process Transaction ID

  if (!strcmp(idTagInfoStatus, "Accepted")) {
    if (DEBUG_APP_LAY) Serial.print(F("[StartTransaction] Request has been accepted!\n")); //, transactionID is "));
    //Serial.print(transactionId);
    //Serial.print(F("\n"));
    completed = true;

    ChargePointStatusService *cpStatusService = getChargePointStatusService();
    if (cpStatusService != NULL){
      cpStatusService->startTransaction(transactionId);
    }

    SmartChargingService *scService = getSmartChargingService();
    if (scService != NULL){
      scService->beginChargingNow();
    }
    
    if (onCompleteListener != NULL){
      onCompleteListener(json);
      onCompleteListener = NULL; //just call once per instance
    }
    waitForConf = false;
  } else {
    Serial.printf("[StartTransaction] Request has been denied!");
  }

  return true; //Message has been consumed
}


/* ####################################
 * For debugging only: implement dummy server functionalities to test against echo server
 * #################################### */
boolean StartTransaction::receiveReq(JsonDocument *json) {

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
boolean StartTransaction::sendConf() {
  if (!reqExecuted) {
    //wait until req has been executed
    return false;
  }

  /*
   * Generate JSON object
   */
  const size_t capacity = JSON_ARRAY_SIZE(3) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + 50;
  DynamicJsonDocument doc(capacity);

  doc.add(MESSAGE_TYPE_CALLRESULT);               //MessageType
  doc.add(this->getMessageID());       //Unique message ID
  JsonObject doc_2 = doc.createNestedObject(); //Payload
  JsonObject idTagInfo = doc_2.createNestedObject("idTagInfo");
  idTagInfo["status"] = "Accepted";
  doc_2["transactionId"] = 123456;

  char out[150];  
  size_t len = serializeJson(doc, out, 150);

  boolean success = webSocket->sendTXT(out, len);
  doc.clear();
  return success;
}
