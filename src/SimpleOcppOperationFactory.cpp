#include "Variants.h"

#include "SimpleOcppOperationFactory.h"

#include "Authorize.h"
#include "BootNotification.h"
#include "Heartbeat.h"
#include "MeterValues.h"
#include "TargetValues.h"
#include "SetChargingProfile.h"
#include "StatusNotification.h"
#include "StartTransaction.h"
#include "StopTransaction.h"
#include "TriggerMessage.h"

#include "OcppEngine.h"

#include <string.h>

void (*onAuthorizeRequest)(JsonDocument *request);
void setOnAuthorizeRequestListener(void (*listener)(JsonDocument *reqeust)){
  onAuthorizeRequest = listener;
}


OnReceiveReqListener onBootNotificationRequest;
void setOnBootNotificationRequestListener(OnReceiveReqListener listener){
  onBootNotificationRequest = listener;
}

OnReceiveReqListener onTargetValuesRequest;
void setOnTargetValuesRequestListener(OnReceiveReqListener listener) {
  onTargetValuesRequest = listener;
}

OnReceiveReqListener onSetChargingProfileRequest;
void setOnSetChargingProfileRequestListener(OnReceiveReqListener listener){
  onSetChargingProfileRequest = listener;
}

OnReceiveReqListener onStartTransactionRequest;
void setOnStartTransactionRequestListener(OnReceiveReqListener listener){
  onStartTransactionRequest = listener;
}

OnReceiveReqListener onTriggerMessageRequest;
void setOnTriggerMessageRequestListener(OnReceiveReqListener listener){
  onTriggerMessageRequest = listener;
}

//TODO unify with makeFromJson
OcppOperation* makeFromTriggerMessage(WebSocketsClient *ws, JsonDocument *json) {
  
  OcppOperation *result = NULL;
  
  JsonObject payload = (*json)[3];
  //int connectorID = payload["connectorId"]; <-- not used in this implementation
  const char *action = payload["requestedMessage"];

  if (DEBUG_OUTPUT) {
    Serial.print(F("[SimpleOcppOperationFactory] makeFromTriggerMessage reads "));
    Serial.print(action);
    Serial.print(F("\n"));
  }

  if (!strcmp(action, "Authorize")) {
    result = new Authorize(ws);
    result->setOnReceiveReq(onAuthorizeRequest);
  } else if (!strcmp(action, "BootNotification")) {
    result = new BootNotification(ws);
    result->setOnReceiveReq(onBootNotificationRequest);
  } else if (!strcmp(action, "Heartbeat")) {
    result = new Heartbeat(ws);
    //result->setOnReceiveReq(onAuthorizeRequest); <-- no parallel functionality because Heartbeat instantiation is for dummy echo test server
  } else if (!strcmp(action, "DataTransfer")) { //patch when more modifications are added
    result = new TargetValues(ws);
    result->setOnReceiveReq(onTargetValuesRequest);
  } else if (!strcmp(action, "SetChargingProfile")) {
    result = new SetChargingProfile(ws, getSmartChargingService());
    result->setOnReceiveReq(onSetChargingProfileRequest);
  } else if (!strcmp(action, "StatusNotification")) {
    result = new StatusNotification(ws);
    //result->setOnReceiveReq(onAuthorizeRequest);
  } else if (!strcmp(action, "StartTransaction")) {
    result = new StartTransaction(ws);
    result->setOnReceiveReq(onStartTransactionRequest); //<-- no parallel functionality because Heartbeat instantiation is for dummy echo test server
  } else if (!strcmp(action, "StopTransaction")) {
    result = new StopTransaction(ws);
    //result->setOnReceiveReq(onAuthorizeRequest); <-- no parallel functionality because Heartbeat instantiation is for dummy echo test server
  } else if (!strcmp(action, "TriggerMessage")) {
    result = new TriggerMessage(ws);
    result->setOnReceiveReq(onTriggerMessageRequest);
  } else {
    Serial.print(F("[SimpleOcppOperationFactory] Operation not supported"));
      //TODO reply error code
  }

  return result;
}

//TODO unify with makeFromTriggerMessage
OcppOperation* makeFromJson(WebSocketsClient *ws, JsonDocument *json){
  OcppOperation *result = NULL;
  
  const char* action = (*json)[2];

  if (!strcmp(action, "Authorize")) {
    result = new Authorize(ws);
    result->setOnReceiveReq(onAuthorizeRequest);
  } else if (!strcmp(action, "BootNotification")) {
    result = new BootNotification(ws);
    result->setOnReceiveReq(onBootNotificationRequest);
  } else if (!strcmp(action, "Heartbeat")) {
    result = new Heartbeat(ws);
    //result->setOnReceiveReq(onAuthorizeRequest); <-- no parallel functionality because Heartbeat instantiation is for dummy echo test server
  } else if (!strcmp(action, "MeterValues")) {
    result = new MeterValues(ws);
    //result->setOnReceiveReq(onTriggerMessageRequest); <-- no parallel functionality because Heartbeat instantiation is for dummy echo test server
  } else if (!strcmp(action, "DataTransfer")) { //patch when more modifications are added
    result = new TargetValues(ws);
    result->setOnReceiveReq(onTargetValuesRequest);
  } else if (!strcmp(action, "SetChargingProfile")) {
    result = new SetChargingProfile(ws, getSmartChargingService());
    result->setOnReceiveReq(onSetChargingProfileRequest);
  } else if (!strcmp(action, "StatusNotification")) {
    result = new StatusNotification(ws);
    //result->setOnReceiveReq(onAuthorizeRequest);
  } else if (!strcmp(action, "StartTransaction")) {
    result = new StartTransaction(ws);
    result->setOnReceiveReq(onStartTransactionRequest);// <-- no parallel functionality because Heartbeat instantiation is for dummy echo test server
  } else if (!strcmp(action, "StopTransaction")) {
    result = new StopTransaction(ws);
    //result->setOnReceiveReq(onAuthorizeRequest); <-- no parallel functionality because Heartbeat instantiation is for dummy echo test server
  } else if (!strcmp(action, "TriggerMessage")) {
    result = new TriggerMessage(ws);
    result->setOnReceiveReq(onTriggerMessageRequest);
  } else {
    Serial.print(F("[SimpleOcppOperationFactory] Operation not supported"));
      //TODO reply error code
  }

  return result;
}
