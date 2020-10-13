// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
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

void setOnAuthorizeRequestListener(void listener(JsonObject payload));
void setOnBootNotificationRequestListener(void listener(JsonObject payload));
void setOnTargetValuesRequestListener(void listener(JsonObject payload));
void setOnSetChargingProfileRequestListener(void listener(JsonObject payload));
void setOnStartTransactionRequestListener(void listener(JsonObject payload));
void setOnTriggerMessageRequestListener(void listener(JsonObject payload));
void setOnRemoteStartTransactionReceiveRequestListener(void listener(JsonObject payload));
void setOnRemoteStartTransactionSendConfListener(void listener(JsonObject payload));
void setOnRemoteStopTransactionSendConfListener(void listener(JsonObject payload));
void setOnResetSendConfListener(void listener(JsonObject payload));
#endif
