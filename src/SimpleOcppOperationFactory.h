#ifndef SIMPLEOCPPOPERATIONFACTORY_H
#define SIMPLEOCPPOPERATIONFACTORY_H

#include <ArduinoJson.h>
#include <WebSocketsClient.h>

#include "OcppOperation.h"

OcppOperation* makeFromTriggerMessage(WebSocketsClient *ws, JsonDocument *request);

OcppOperation* makeFromJson(WebSocketsClient *ws, JsonDocument *request);

void setOnAuthorizeRequestListener(void listener(JsonDocument *reqeust));
void setOnBootNotificationRequestListener(void listener(JsonDocument *reqeust));
void setOnTargetValuesRequestListener(void listener(JsonDocument *reqeust));
void setOnSetChargingProfileRequestListener(void listener(JsonDocument *reqeust));
void setOnStartTransactionRequestListener(void listener(JsonDocument *reqeust));
void setOnTriggerMessageRequestListener(void listener(JsonDocument *reqeust));
#endif
