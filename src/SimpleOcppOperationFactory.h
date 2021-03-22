// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef SIMPLEOCPPOPERATIONFACTORY_H
#define SIMPLEOCPPOPERATIONFACTORY_H

#include <ArduinoJson.h>
#include <WebSocketsClient.h>

#include "OcppOperation.h"

OcppOperation* makeFromTriggerMessage(WebSocketsClient *ws, JsonObject payload);

OcppOperation* makeFromJson(WebSocketsClient *ws, JsonDocument *request);

OcppOperation* makeOcppOperation(WebSocketsClient *ws);

OcppOperation* makeOcppOperation(WebSocketsClient *ws, OcppMessage *msg);

OcppOperation *makeOcppOperation(WebSocketsClient *ws, const char *actionCode);

void setOnAuthorizeRequestListener(OnReceiveReqListener onReceiveReq);
void setOnBootNotificationRequestListener(OnReceiveReqListener onReceiveReq);
void setOnTargetValuesRequestListener(OnReceiveReqListener onReceiveReq);
void setOnSetChargingProfileRequestListener(OnReceiveReqListener onReceiveReq);
void setOnStartTransactionRequestListener(OnReceiveReqListener onReceiveReq);
void setOnTriggerMessageRequestListener(OnReceiveReqListener onReceiveReq);
void setOnRemoteStartTransactionReceiveRequestListener(OnReceiveReqListener onReceiveReq);
void setOnRemoteStartTransactionSendConfListener(OnSendConfListener onSendConf);
void setOnRemoteStopTransactionSendConfListener(OnSendConfListener onSendConf);
void setOnChangeConfigurationReceiveRequestListener(OnReceiveReqListener onReceiveReq);
void setOnChangeConfigurationSendConfListener(OnSendConfListener onSendConf);
void setOnGetConfigurationReceiveRequestListener(OnReceiveReqListener onReceiveReq);
void setOnGetConfigurationSendConfListener(OnSendConfListener onSendConf);
void setOnResetSendConfListener(OnSendConfListener onSendConf);
void setOnUpdateFirmwareReceiveRequestListener(OnReceiveReqListener onReceiveReq);
#endif
