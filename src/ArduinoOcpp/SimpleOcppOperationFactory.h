// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef SIMPLEOCPPOPERATIONFACTORY_H
#define SIMPLEOCPPOPERATIONFACTORY_H

#include <ArduinoJson.h>

#include <ArduinoOcpp/Core/OcppOperation.h>

namespace ArduinoOcpp {

typedef std::function<OcppMessage*()> OcppMessageCreator;

OcppOperation* makeFromTriggerMessage(JsonObject payload);

OcppOperation* makeFromJson(JsonDocument *request);

OcppOperation* makeOcppOperation();

OcppOperation* makeOcppOperation(OcppMessage *msg);

OcppOperation *makeOcppOperation(const char *actionCode);

void registerCustomOcppMessage(const char *messageType, OcppMessageCreator ocppMessageCreator, OnReceiveReqListener onReceiveReq = NULL);

void setOnAuthorizeRequestListener(OnReceiveReqListener onReceiveReq);
void setOnBootNotificationRequestListener(OnReceiveReqListener onReceiveReq);
void setOnTargetValuesRequestListener(OnReceiveReqListener onReceiveReq);
void setOnSetChargingProfileRequestListener(OnReceiveReqListener onReceiveReq);
void setOnStartTransactionRequestListener(OnReceiveReqListener onReceiveReq);
void setOnTriggerMessageRequestListener(OnReceiveReqListener onReceiveReq);
void setOnRemoteStartTransactionReceiveRequestListener(OnReceiveReqListener onReceiveReq);
void setOnRemoteStartTransactionSendConfListener(OnSendConfListener onSendConf);
void setOnRemoteStopTransactionReceiveRequestListener(OnReceiveReqListener onReceiveReq);
void setOnRemoteStopTransactionSendConfListener(OnSendConfListener onSendConf);
void setOnChangeConfigurationReceiveRequestListener(OnReceiveReqListener onReceiveReq);
void setOnChangeConfigurationSendConfListener(OnSendConfListener onSendConf);
void setOnGetConfigurationReceiveRequestListener(OnReceiveReqListener onReceiveReq);
void setOnGetConfigurationSendConfListener(OnSendConfListener onSendConf);
void setOnResetSendConfListener(OnSendConfListener onSendConf);
void setOnUpdateFirmwareReceiveRequestListener(OnReceiveReqListener onReceiveReq);

} //end namespace ArduinoOcpp
#endif
