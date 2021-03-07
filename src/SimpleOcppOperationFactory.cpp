// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include "Variants.h"

#include "SimpleOcppOperationFactory.h"

#include "Authorize.h"
#include "BootNotification.h"
#include "Heartbeat.h"
#include "MeterValues.h"
#include "SetChargingProfile.h"
#include "StatusNotification.h"
#include "StartTransaction.h"
#include "StopTransaction.h"
#include "TriggerMessage.h"
#include "RemoteStartTransaction.h"
#include "OcppError.h"
#include "RemoteStopTransaction.h"
#include "ChangeConfiguration.h"
#include "GetConfiguration.h"
#include "Reset.h"

#include "OcppEngine.h"

#include <string.h>

OnReceiveReqListener onAuthorizeRequest;
void setOnAuthorizeRequestListener(OnReceiveReqListener listener){
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

OnReceiveReqListener onRemoteStartTransactionReceiveRequest;
void setOnRemoteStartTransactionReceiveRequestListener(OnReceiveReqListener listener) {
  onRemoteStartTransactionReceiveRequest = listener;
}

OnSendConfListener onRemoteStartTransactionSendConf;
void setOnRemoteStartTransactionSendConfListener(OnSendConfListener listener){
  onRemoteStartTransactionSendConf = listener;
}

OnSendConfListener onRemoteStopTransactionSendConf;
void setOnRemoteStopTransactionSendConfListener(OnSendConfListener listener){
  onRemoteStopTransactionSendConf = listener;
}

OnSendConfListener onChangeConfigurationReceiveReq;
void setOnChangeConfigurationReceiveRequestListener(OnSendConfListener listener){
  onChangeConfigurationReceiveReq = listener;
}

OnSendConfListener onChangeConfigurationSendConf;
void setOnChangeConfigurationSendConfListener(OnSendConfListener listener){
  onChangeConfigurationSendConf = listener;
}

OnSendConfListener onGetConfigurationReceiveReq;
void setOnGetConfigurationReceiveReqListener(OnSendConfListener listener){
  onGetConfigurationReceiveReq = listener;
}

OnSendConfListener onGetConfigurationSendConf;
void setOnGetConfigurationSendConfListener(OnSendConfListener listener){
  onGetConfigurationSendConf = listener;
}

OnSendConfListener onResetSendConf;
void setOnResetSendConfListener(OnSendConfListener listener){
  onResetSendConf = listener;
}

OcppOperation* makeFromTriggerMessage(WebSocketsClient *ws, JsonObject payload) {

  //int connectorID = payload["connectorId"]; <-- not used in this implementation
  const char *messageType = payload["requestedMessage"];

  if (DEBUG_OUT) {
    Serial.print(F("[SimpleOcppOperationFactory] makeFromTriggerMessage for message type "));
    Serial.print(messageType);
    Serial.print(F("\n"));
  }

  return makeOcppOperation(ws, messageType);
}

OcppOperation *makeFromJson(WebSocketsClient *ws, JsonDocument *json) {
  const char* messageType = (*json)[2];
  return makeOcppOperation(ws, messageType);
}

OcppOperation *makeOcppOperation(WebSocketsClient *ws, const char *messageType) {
  OcppOperation *operation = makeOcppOperation(ws);
  OcppMessage *msg = NULL;

  if (!strcmp(messageType, "Authorize")) {
    msg = new Authorize();
    operation->setOnReceiveReqListener(onAuthorizeRequest);
  } else if (!strcmp(messageType, "BootNotification")) {
    msg = new BootNotification();
    operation->setOnReceiveReqListener(onBootNotificationRequest);
  } else if (!strcmp(messageType, "Heartbeat")) {
    msg = new Heartbeat();
  } else if (!strcmp(messageType, "MeterValues")) {
    msg = new MeterValues();
  } else if (!strcmp(messageType, "SetChargingProfile")) {
    msg = new SetChargingProfile(getSmartChargingService());
    operation->setOnReceiveReqListener(onSetChargingProfileRequest);
  } else if (!strcmp(messageType, "StatusNotification")) {
    msg = new StatusNotification();
  } else if (!strcmp(messageType, "StartTransaction")) {
    msg = new StartTransaction(1); //connectorId 1
    operation->setOnReceiveReqListener(onStartTransactionRequest);
  } else if (!strcmp(messageType, "StopTransaction")) {
    msg = new StopTransaction();
  } else if (!strcmp(messageType, "TriggerMessage")) {
    msg = new TriggerMessage(ws);
    operation->setOnReceiveReqListener(onTriggerMessageRequest);
  } else if (!strcmp(messageType, "RemoteStartTransaction")) {
    msg = new RemoteStartTransaction();
    operation->setOnReceiveReqListener(onRemoteStartTransactionReceiveRequest);
    if (onRemoteStartTransactionSendConf == NULL) 
      Serial.print(F("[SimpleOcppOperationFactory] Warning: RemoteStartTransaction is without effect when the sendConf listener is not set. Set a listener which initiates the StartTransaction operation.\n"));
    operation->setOnSendConfListener(onRemoteStartTransactionSendConf);
  } else if (!strcmp(messageType, "RemoteStopTransaction")) {
    msg = new RemoteStopTransaction();
    if (onRemoteStopTransactionSendConf == NULL) 
      Serial.print(F("[SimpleOcppOperationFactory] Warning: RemoteStopTransaction is without effect when the sendConf listener is not set. Set a listener which initiates the StopTransaction operation.\n"));
    operation->setOnSendConfListener(onRemoteStopTransactionSendConf);
//#endif
  } else if (!strcmp(messageType, "ChangeConfiguration")) {
    msg = new ChangeConfiguration();
    operation->setOnReceiveReqListener(onChangeConfigurationReceiveReq);
    operation->setOnSendConfListener(onChangeConfigurationSendConf);
  } else if (!strcmp(messageType, "GetConfiguration")) {
    msg = new GetConfiguration();
    operation->setOnReceiveReqListener(onGetConfigurationReceiveReq);
    operation->setOnSendConfListener(onGetConfigurationSendConf);
  } else if (!strcmp(messageType, "Reset")) {
    msg = new Reset();
    if (onResetSendConf == NULL)
      Serial.print(F("[SimpleOcppOperationFactory] Warning: Reset is without effect when the sendConf listener is not set. Set a listener which resets your device.\n"));
    operation->setOnSendConfListener(onResetSendConf);
  } else {
    Serial.println(F("[SimpleOcppOperationFactory] Operation not supported"));
    msg = new NotImplemented();
  }

  if (msg == NULL) {
    delete operation;
    return NULL;
  } else {
    operation->setOcppMessage(msg);
    return operation;
  }
}

OcppOperation* makeOcppOperation(WebSocketsClient *ws, OcppMessage *msg){
  if (msg == NULL) {
    Serial.print(F("[SimpleOcppOperationFactory] in makeOcppOperation(webSocket, ocppMessage): ocppMessage is null!\n"));
    return NULL;
  }
  OcppOperation *operation = makeOcppOperation(ws);
  operation->setOcppMessage(msg);
  return operation;
}

OcppOperation* makeOcppOperation(WebSocketsClient *ws){
  OcppOperation *result = new OcppOperation(ws);
  return result;
}
