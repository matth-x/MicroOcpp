// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include "Variants.h"

#include <ArduinoOcpp/SimpleOcppOperationFactory.h>

#include <ArduinoOcpp/MessagesV16/Authorize.h>
#include <ArduinoOcpp/MessagesV16/BootNotification.h>
#include <ArduinoOcpp/MessagesV16/Heartbeat.h>
#include <ArduinoOcpp/MessagesV16/MeterValues.h>
#include <ArduinoOcpp/MessagesV16/SetChargingProfile.h>
#include <ArduinoOcpp/MessagesV16/StatusNotification.h>
#include <ArduinoOcpp/MessagesV16/StartTransaction.h>
#include <ArduinoOcpp/MessagesV16/StopTransaction.h>
#include <ArduinoOcpp/MessagesV16/TriggerMessage.h>
#include <ArduinoOcpp/MessagesV16/RemoteStartTransaction.h>
#include <ArduinoOcpp/Core/OcppError.h>
#include <ArduinoOcpp/MessagesV16/RemoteStopTransaction.h>
#include <ArduinoOcpp/MessagesV16/ChangeConfiguration.h>
#include <ArduinoOcpp/MessagesV16/GetConfiguration.h>
#include <ArduinoOcpp/MessagesV16/Reset.h>
#include <ArduinoOcpp/MessagesV16/UpdateFirmware.h>
#include <ArduinoOcpp/MessagesV16/FirmwareStatusNotification.h>

#include <ArduinoOcpp/Core/OcppEngine.h>

#include <string.h>

namespace ArduinoOcpp {

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

OnReceiveReqListener onRemoteStopTransactionReceiveRequest;
void setOnRemoteStopTransactionReceiveRequestListener(OnReceiveReqListener listener){
  onRemoteStopTransactionReceiveRequest = listener;
}

OnSendConfListener onRemoteStopTransactionSendConf;
void setOnRemoteStopTransactionSendConfListener(OnSendConfListener listener){
  onRemoteStopTransactionSendConf = listener;
}

OnSendConfListener onChangeConfigurationReceiveReq;
void setOnChangeConfigurationReceiveRequestListener(OnReceiveReqListener listener){
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

OnReceiveReqListener onUpdateFirmwareReceiveReq;
void setOnUpdateFirmwareReceiveRequestListener(OnReceiveReqListener listener) {
  onUpdateFirmwareReceiveReq = listener;
}

struct CustomOcppMessageCreatorEntry {
    const char *messageType;
    OcppMessageCreator creator;
    OnReceiveReqListener onReceiveReq;
};

std::vector<CustomOcppMessageCreatorEntry> customMessagesRegistry;
void registerCustomOcppMessage(const char *messageType, OcppMessageCreator ocppMessageCreator, OnReceiveReqListener onReceiveReq) {
    customMessagesRegistry.erase(std::remove_if(customMessagesRegistry.begin(),
                                               customMessagesRegistry.end(),
            [messageType](CustomOcppMessageCreatorEntry &el) {
                return !strcmp(messageType, el.messageType);
            }),
        customMessagesRegistry.end());

    CustomOcppMessageCreatorEntry entry;
    entry.messageType = messageType;
    entry.creator  = ocppMessageCreator;
    entry.onReceiveReq = onReceiveReq;

    customMessagesRegistry.push_back(entry);
}

CustomOcppMessageCreatorEntry *makeCustomOcppMessage(const char *messageType) {
    for (auto it = customMessagesRegistry.begin(); it != customMessagesRegistry.end(); ++it) {
        if (!strcmp(it->messageType, messageType)) {
            return &(*it);
        }
    }
    return NULL;
}

OcppOperation* makeFromTriggerMessage(JsonObject payload) {

  //int connectorID = payload["connectorId"]; <-- not used in this implementation
  const char *messageType = payload["requestedMessage"];

  if (DEBUG_OUT) {
    Serial.print(F("[SimpleOcppOperationFactory] makeFromTriggerMessage for message type "));
    Serial.print(messageType);
    Serial.print(F("\n"));
  }

  return makeOcppOperation(messageType);
}

OcppOperation *makeFromJson(JsonDocument *json) {
  const char* messageType = (*json)[2];
  return makeOcppOperation(messageType);
}

OcppOperation *makeOcppOperation(const char *messageType) {
  OcppOperation *operation = makeOcppOperation();
  OcppMessage *msg = NULL;

  if (CustomOcppMessageCreatorEntry *entry = makeCustomOcppMessage(messageType)) {
      msg = entry->creator();
      operation->setOnReceiveReqListener(entry->onReceiveReq);
  } else if (!strcmp(messageType, "Authorize")) {
    msg = new Ocpp16::Authorize();
    operation->setOnReceiveReqListener(onAuthorizeRequest);
  } else if (!strcmp(messageType, "BootNotification")) {
    msg = new Ocpp16::BootNotification();
    operation->setOnReceiveReqListener(onBootNotificationRequest);
  } else if (!strcmp(messageType, "Heartbeat")) {
    msg = new Ocpp16::Heartbeat();
  } else if (!strcmp(messageType, "MeterValues")) {
    msg = new Ocpp16::MeterValues();
  } else if (!strcmp(messageType, "SetChargingProfile")) {
    msg = new Ocpp16::SetChargingProfile(getSmartChargingService());
    operation->setOnReceiveReqListener(onSetChargingProfileRequest);
  } else if (!strcmp(messageType, "StatusNotification")) {
    msg = new Ocpp16::StatusNotification();
  } else if (!strcmp(messageType, "StartTransaction")) {
    msg = new Ocpp16::StartTransaction(1); //connectorId 1
    operation->setOnReceiveReqListener(onStartTransactionRequest);
  } else if (!strcmp(messageType, "StopTransaction")) {
    msg = new Ocpp16::StopTransaction();
  } else if (!strcmp(messageType, "TriggerMessage")) {
    msg = new Ocpp16::TriggerMessage();
    operation->setOnReceiveReqListener(onTriggerMessageRequest);
  } else if (!strcmp(messageType, "RemoteStartTransaction")) {
    msg = new Ocpp16::RemoteStartTransaction();
    operation->setOnReceiveReqListener(onRemoteStartTransactionReceiveRequest);
    if (onRemoteStartTransactionSendConf == NULL) 
      Serial.print(F("[SimpleOcppOperationFactory] Warning: RemoteStartTransaction is without effect when the sendConf listener is not set. Set a listener which initiates the StartTransaction operation.\n"));
    operation->setOnSendConfListener(onRemoteStartTransactionSendConf);
  } else if (!strcmp(messageType, "RemoteStopTransaction")) {
    msg = new Ocpp16::RemoteStopTransaction();
    if (onRemoteStopTransactionSendConf == NULL) 
      Serial.print(F("[SimpleOcppOperationFactory] Warning: RemoteStopTransaction is without effect when no sendConf listener is set. Set a listener which initiates the StopTransaction operation.\n"));
    operation->setOnReceiveReqListener(onRemoteStopTransactionReceiveRequest);
    operation->setOnSendConfListener(onRemoteStopTransactionSendConf);
  } else if (!strcmp(messageType, "ChangeConfiguration")) {
    msg = new Ocpp16::ChangeConfiguration();
    operation->setOnReceiveReqListener(onChangeConfigurationReceiveReq);
    operation->setOnSendConfListener(onChangeConfigurationSendConf);
  } else if (!strcmp(messageType, "GetConfiguration")) {
    msg = new Ocpp16::GetConfiguration();
    operation->setOnReceiveReqListener(onGetConfigurationReceiveReq);
    operation->setOnSendConfListener(onGetConfigurationSendConf);
  } else if (!strcmp(messageType, "Reset")) {
    msg = new Ocpp16::Reset();
    if (onResetSendConf == NULL)
      Serial.print(F("[SimpleOcppOperationFactory] Warning: Reset is without effect when the sendConf listener is not set. Set a listener which resets your device.\n"));
    operation->setOnSendConfListener(onResetSendConf);
  } else if (!strcmp(messageType, "UpdateFirmware")) {
    msg = new Ocpp16::UpdateFirmware();
    if (onUpdateFirmwareReceiveReq == NULL)
      Serial.print(F("[SimpleOcppOperationFactory] Warning: UpdateFirmware is without effect when the receiveReq listener is not set. Please implement a FW update routine for your device.\n"));
    operation->setOnReceiveReqListener(onUpdateFirmwareReceiveReq);
  } else if (!strcmp(messageType, "FirmwareStatusNotification")) {
    msg = new Ocpp16::FirmwareStatusNotification();
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

OcppOperation* makeOcppOperation(OcppMessage *msg){
  if (msg == NULL) {
    Serial.print(F("[SimpleOcppOperationFactory] in makeOcppOperation(webSocket, ocppMessage): ocppMessage is null!\n"));
    return NULL;
  }
  OcppOperation *operation = makeOcppOperation();
  operation->setOcppMessage(msg);
  return operation;
}

OcppOperation* makeOcppOperation(){
  OcppOperation *result = new OcppOperation();
  return result;
}

} //end namespace ArduinoOcpp
