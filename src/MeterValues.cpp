#include "MeterValues.h"
#include "OcppEngine.h"
#include "MeteringService.h"

#include "Variants.h"

//can only be used for echo server debugging
MeterValues::MeterValues(WebSocketsClient *webSocket)
    : OcppOperation(webSocket) {
  powerMeasurementTime = LinkedList<time_t>(); //not used in server mode but needed to keep the destructor simple
  power = LinkedList<float>();
}

MeterValues::MeterValues(WebSocketsClient *webSocket, LinkedList<time_t> *pwrMeasurementTime, LinkedList<float> *pwr)
    : OcppOperation(webSocket) {
  powerMeasurementTime = LinkedList<time_t>();
  power = LinkedList<float>();
  for (int i = 0; i < pwr->size(); i++) {
    powerMeasurementTime.add(pwrMeasurementTime->get(i));
    power.add((float) pwr->get(i));
  }
}

MeterValues::~MeterValues(){
  powerMeasurementTime.clear();
  power.clear();
}

boolean MeterValues::sendReq() {
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
  const size_t capacity = JSON_ARRAY_SIZE(4) //Header including e.g. messageId
      + JSON_OBJECT_SIZE(1) //payload
      + JSON_ARRAY_SIZE(METER_VALUES_SAMPLED_DATA_MAX_LENGTH) //metervalues array
      + METER_VALUES_SAMPLED_DATA_MAX_LENGTH * (JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(1)) //metervalues array contents
      + 150; //"safety space"
  const int OUT_LEN = 512;
  DynamicJsonDocument doc(capacity);

  doc.add(MESSAGE_TYPE_CALL);      //MessageType
  doc.add(this->getMessageID());   //Unique message ID
  doc.add("MeterValues");            //Action
  JsonObject payload = doc.createNestedObject(); //Payload
  payload["connectorId"] = 1;
  if (getChargePointStatusService() != NULL) {
    payload["transactionId"] = getChargePointStatusService()->getTransactionId();
  }
  JsonArray meterValues = payload.createNestedArray("meterValue");
  for (int i = 0; i < power.size(); i++) {
    JsonObject meterValue = meterValues.createNestedObject();
    char timestamp[JSONDATE_LENGTH + 1] = {'\0'};
    getJsonDateStringFromGivenTime(timestamp, JSONDATE_LENGTH + 1, powerMeasurementTime.get(i));
    meterValue["timestamp"] = timestamp;
    meterValue["sampledValue"]["value"] = power.get(i);
  }

  char out[OUT_LEN];  
  size_t len = serializeJson(doc, out, OUT_LEN);

  if (DEBUG_APP_LAY) Serial.print(F("[MeterValues] JSON message: "));
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

boolean MeterValues::receiveConf(JsonDocument *json) {

  /**
   * Check if conf-message belongs to this operation instance
   */
  
  if ((*json)[1] != this->getMessageID()) {
    return false;
  }

  /**
   * Process the message
   */

  if (DEBUG_APP_LAY) Serial.print(F("[MeterValues] Request has been accepted!\n"));
  completed = true;
  if (onCompleteListener != NULL){
    onCompleteListener(json);
    onCompleteListener = NULL; //just call once
  }

  return true; //Message has been consumed
}


/* ####################################
 * For debugging only: implement dummy server functionalities to test against echo server
 * #################################### */
boolean MeterValues::receiveReq(JsonDocument *json) {

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
boolean MeterValues::sendConf() {
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
