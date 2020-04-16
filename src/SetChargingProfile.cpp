#include "Variants.h"

#include "SetChargingProfile.h"

SetChargingProfile::SetChargingProfile(WebSocketsClient *webSocket, SmartChargingService *smartChargingService) 
  : OcppOperation(webSocket) {
  this->smartChargingService = smartChargingService;
}

boolean SetChargingProfile::receiveReq(JsonDocument *json) {

  this->setMessageID((*json)[1]);

  if (DEBUG_APP_LAY) Serial.print(F("[SetChargingProfile] Receiving request\n"));

  /**
   * Process the message
   */

  JsonObject payload = (*json)[3];

  int connectorID = payload["connectorId"]; //<-- not used in this implementation

  JsonObject csChargingProfiles = payload["csChargingProfiles"];

  smartChargingService->updateChargingProfile(&csChargingProfiles);

  if (onReceiveReqListener != NULL){
      onReceiveReqListener(json);
      onReceiveReqListener = NULL; //just call once
  }

  reqExecuted = true;
  
  return true; //Message has been consumed
}

boolean SetChargingProfile::sendConf() {
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
  doc_2["status"] = "Accepted";

  char out[OUT_LEN];  
  size_t len = serializeJson(doc, out, OUT_LEN);

  boolean success = webSocket->sendTXT(out, len);
  if (success) {
    if (DEBUG_APP_LAY) Serial.print(F("[SetChargingProfile] (Conf) JSON message: "));
    if (DEBUG_APP_LAY) Serial.printf(out);
    if (DEBUG_APP_LAY) Serial.print('\n');
    if (onCompleteListener != NULL){
      onCompleteListener(&doc);
      onCompleteListener = NULL; //just call once
    }
  }
  doc.clear();
  return success;
}
