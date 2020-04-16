#include "Variants.h"

#include "TargetValues.h"
#include "OcppEngine.h"
#include "TimeHelper.h"


TargetValues::TargetValues(WebSocketsClient *webSocket) 
  : OcppOperation(webSocket) {
  
}

boolean TargetValues::sendReq() {
  if (completed) {
    //StartTransaction request has been conf'd and is therefore completed
    return true;
  }
  if (waitForConf) {
    //StartTransaction request is already pending; nothing to do here
    return false;
  }

  if (DEBUG_APP_LAY) Serial.print(F("[TargetValues] Request embedded in a DataTransfer request with dummy data\n"));
  
  /*
   * Generate JSON object
   */
  const size_t capacity = JSON_ARRAY_SIZE(4) + 3*JSON_OBJECT_SIZE(2) + 2*JSON_OBJECT_SIZE(3) + 50;
  DynamicJsonDocument doc(capacity);

  doc.add(MESSAGE_TYPE_CALL);      //MessageType
  doc.add(this->getMessageID());   //Unique message ID
  doc.add("DataTransfer");         //Action = DataTransfer as part of OCPP 1.6. 

  JsonObject payload = doc.createNestedObject();
  payload["vendorId"] = "customVendorId";
  payload["messageId"] = "TargetValues";

  JsonObject payload_data = payload.createNestedObject("data"); //Contents of payload_data are as specified in OCPP 2.0 TargetValues

  int transactionId = -1;
  ChargePointStatusService *cpStatusService = getChargePointStatusService();
    if (cpStatusService != NULL){
      transactionId = cpStatusService->getTransactionId();
    }
  payload_data["transactionId"] = transactionId;

  JsonObject payload_data_chargingNeeds = payload_data.createNestedObject("chargingNeeds");

  /*
   * Fill TargetValues with dummy data for now
   */

  char departureTime[JSONDATE_LENGTH + 1] = {'\0'};
  getJsonDateStringFromGivenTime(departureTime, JSONDATE_LENGTH, now() + (time_t) (2 * SECS_PER_HOUR)); //departure time is set to be in 2 hours
  payload_data_chargingNeeds["departureTime"] = departureTime;

  //if AC charging mode ...
  JsonObject payload_data_chargingNeeds_acChargingParameters = payload_data_chargingNeeds.createNestedObject("acChargingParameters");
  payload_data_chargingNeeds_acChargingParameters["energyAmount"] = 10000; //defined to be in Wh by OCPP 2.0
  payload_data_chargingNeeds_acChargingParameters["evMaxCurrent"] = 32; //... Amps

  //else if DC charging mode ...
  JsonObject payload_data_chargingNeeds_dcChargingParameters = payload_data_chargingNeeds.createNestedObject("dcChargingParameters");
  payload_data_chargingNeeds_dcChargingParameters["energyAmount"] = 10000; //Wh
  payload_data_chargingNeeds_dcChargingParameters["evMaxPower"] = 22000; //W

  //... end if

  char out[500];  
  size_t len = serializeJson(doc, out, 500);

  if (DEBUG_APP_LAY) Serial.print(F("[TargetValues] JSON message: "));
  if (DEBUG_APP_LAY) Serial.printf(out);
  if (DEBUG_APP_LAY) Serial.print('\n');

  doc.clear();

  boolean success = webSocket->sendTXT(out, len);
  if (success) {
    waitForConf = true; //package has been sent and waits for its answer
  }
  return false;
}

boolean TargetValues::receiveConf(JsonDocument *json) {

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

  //Right now, no status information is included in this message type. Maybe,
  //status like "Not satisfiable" would be interesting here

  if (DEBUG_APP_LAY) Serial.print(F("[TargetValues] Request has been accepted!\n"));
  
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
boolean TargetValues::receiveReq(JsonDocument *json) {

  this->setMessageID((*json)[1]);

  /**
   * Ignore Contents of this Req-message, because this is for debug purposes only. Otherwise the Target Values would
   * be processed here
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
boolean TargetValues::sendConf() {
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
  doc.add(this->getMessageID());                  //Unique message ID
  JsonObject doc_2 = doc.createNestedObject();    //Payload
  doc_2["status"] = "Accepted"; //Could be extended to transmit status like "Not satisfiable"

  char out[150];  
  size_t len = serializeJson(doc, out, 150);

  boolean success = webSocket->sendTXT(out, len);
  doc.clear();
  return success;
}
