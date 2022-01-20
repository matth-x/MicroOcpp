// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef SIMPLEOCPPOPERATIONFACTORY_H
#define SIMPLEOCPPOPERATIONFACTORY_H

#include <ArduinoJson.h>
#include <ArduinoOcpp/Core/OcppOperation.h>

namespace ArduinoOcpp {

using OcppMessageCreator = std::function<OcppMessage*()>;

std::unique_ptr<OcppOperation> makeFromTriggerMessage(JsonObject payload);

std::unique_ptr<OcppOperation> makeFromJson(const JsonDocument& request);

std::unique_ptr<OcppOperation> makeOcppOperation();

std::unique_ptr<OcppOperation> makeOcppOperation(OcppMessage *msg);

std::unique_ptr<OcppOperation> makeOcppOperation(const char *actionCode);

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
void setOnResetReceiveRequestListener(OnReceiveReqListener onReceiveReq);
void setOnResetSendConfListener(OnSendConfListener onSendConf);
void setOnUpdateFirmwareReceiveRequestListener(OnReceiveReqListener onReceiveReq);
void setOnMeterValuesReceiveRequestListener(OnReceiveReqListener onReceiveReq);

void simpleOcppFactory_deinitialize();

} //end namespace ArduinoOcpp
#endif
