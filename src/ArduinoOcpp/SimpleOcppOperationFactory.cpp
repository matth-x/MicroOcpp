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
#include <ArduinoOcpp/MessagesV16/GetDiagnostics.h>
#include <ArduinoOcpp/MessagesV16/DiagnosticsStatusNotification.h>
#include <ArduinoOcpp/MessagesV16/UnlockConnector.h>
#include <ArduinoOcpp/MessagesV16/ClearChargingProfile.h>
#include <ArduinoOcpp/MessagesV16/ChangeAvailability.h>

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

OnSendConfListener onResetReceiveReq;
void setOnResetReceiveRequestListener(OnReceiveReqListener listener) {
    onResetReceiveReq = listener;
}

OnSendConfListener onResetSendConf;
void setOnResetSendConfListener(OnSendConfListener listener){
    onResetSendConf = listener;
}

OnReceiveReqListener onUpdateFirmwareReceiveReq;
void setOnUpdateFirmwareReceiveRequestListener(OnReceiveReqListener listener) {
    onUpdateFirmwareReceiveReq = listener;
}

OnReceiveReqListener onMeterValuesReceiveReq;
void setOnMeterValuesReceiveRequestListener(OnReceiveReqListener listener) {
    onMeterValuesReceiveReq = listener;
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
    return nullptr;
}

std::unique_ptr<OcppOperation> makeFromTriggerMessage(JsonObject payload) {

    //int connectorID = payload["connectorId"]; <-- not used in this implementation
    const char *messageType = payload["requestedMessage"];

    if (DEBUG_OUT) {
        Serial.print(F("[SimpleOcppOperationFactory] makeFromTriggerMessage for message type "));
        Serial.print(messageType);
        Serial.print(F("\n"));
    }

    return makeOcppOperation(messageType);
}

std::unique_ptr<OcppOperation> makeFromJson(const JsonDocument& json) {
    const char* messageType = json[2];
    return makeOcppOperation(messageType);
}

std::unique_ptr<OcppOperation> makeOcppOperation(const char *messageType) {
    auto operation = makeOcppOperation();
    auto msg = std::unique_ptr<OcppMessage>{nullptr};

    if (CustomOcppMessageCreatorEntry *entry = makeCustomOcppMessage(messageType)) {
        msg = std::unique_ptr<OcppMessage>(entry->creator());
        operation->setOnReceiveReqListener(entry->onReceiveReq);
    } else if (!strcmp(messageType, "Authorize")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::Authorize());
        operation->setOnReceiveReqListener(onAuthorizeRequest);
    } else if (!strcmp(messageType, "BootNotification")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::BootNotification());
        operation->setOnReceiveReqListener(onBootNotificationRequest);
    } else if (!strcmp(messageType, "Heartbeat")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::Heartbeat());
    } else if (!strcmp(messageType, "MeterValues")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::MeterValues());
        operation->setOnReceiveReqListener(onMeterValuesReceiveReq);
    } else if (!strcmp(messageType, "SetChargingProfile")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::SetChargingProfile(getSmartChargingService()));
        operation->setOnReceiveReqListener(onSetChargingProfileRequest);
    } else if (!strcmp(messageType, "StatusNotification")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::StatusNotification());
    } else if (!strcmp(messageType, "StartTransaction")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::StartTransaction(1)); //connectorId 1
        operation->setOnReceiveReqListener(onStartTransactionRequest);
    } else if (!strcmp(messageType, "StopTransaction")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::StopTransaction());
    } else if (!strcmp(messageType, "TriggerMessage")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::TriggerMessage());
        operation->setOnReceiveReqListener(onTriggerMessageRequest);
    } else if (!strcmp(messageType, "RemoteStartTransaction")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::RemoteStartTransaction());
        operation->setOnReceiveReqListener(onRemoteStartTransactionReceiveRequest);
        if (onRemoteStartTransactionSendConf == nullptr) 
            Serial.print(F("[SimpleOcppOperationFactory] Warning: RemoteStartTransaction is without effect when the sendConf listener is not set. Set a listener which initiates the StartTransaction operation.\n"));
        operation->setOnSendConfListener(onRemoteStartTransactionSendConf);
    } else if (!strcmp(messageType, "RemoteStopTransaction")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::RemoteStopTransaction());
        if (onRemoteStopTransactionSendConf == nullptr) 
            Serial.print(F("[SimpleOcppOperationFactory] Warning: RemoteStopTransaction is without effect when no sendConf listener is set. Set a listener which initiates the StopTransaction operation.\n"));
        operation->setOnReceiveReqListener(onRemoteStopTransactionReceiveRequest);
        operation->setOnSendConfListener(onRemoteStopTransactionSendConf);
    } else if (!strcmp(messageType, "ChangeConfiguration")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::ChangeConfiguration());
        operation->setOnReceiveReqListener(onChangeConfigurationReceiveReq);
        operation->setOnSendConfListener(onChangeConfigurationSendConf);
    } else if (!strcmp(messageType, "GetConfiguration")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::GetConfiguration());
        operation->setOnReceiveReqListener(onGetConfigurationReceiveReq);
        operation->setOnSendConfListener(onGetConfigurationSendConf);
    } else if (!strcmp(messageType, "Reset")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::Reset());
        if (onResetSendConf == nullptr && onResetReceiveReq == nullptr)
            Serial.print(F("[SimpleOcppOperationFactory] Warning: Reset is without effect when the sendConf and receiveReq listener is not set. Set a listener which resets your device.\n"));
        operation->setOnReceiveReqListener(onResetReceiveReq);
        operation->setOnSendConfListener(onResetSendConf);
    } else if (!strcmp(messageType, "UpdateFirmware")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::UpdateFirmware());
        //if (onUpdateFirmwareReceiveReq == nullptr)
        //  Serial.print(F("[SimpleOcppOperationFactory] Warning: UpdateFirmware is without effect when the receiveReq listener is not set. Please implement a FW update routine for your device.\n"));
        operation->setOnReceiveReqListener(onUpdateFirmwareReceiveReq);
    } else if (!strcmp(messageType, "FirmwareStatusNotification")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::FirmwareStatusNotification());
    } else if (!strcmp(messageType, "GetDiagnostics")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::GetDiagnostics());
    } else if (!strcmp(messageType, "DiagnosticsStatusNotification")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::DiagnosticsStatusNotification());
    } else if (!strcmp(messageType, "UnlockConnector")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::UnlockConnector());
    } else if (!strcmp(messageType, "ClearChargingProfile")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::ClearChargingProfile());
    } else if (!strcmp(messageType, "ChangeAvailability")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::ChangeAvailability());
    } else {
        Serial.println(F("[SimpleOcppOperationFactory] Operation not supported"));
        msg = std::unique_ptr<OcppMessage>(new NotImplemented());
    }

    if (msg == nullptr) {
        return nullptr;
    } else {
        operation->setOcppMessage(std::move(msg));
        return operation;
    }
}

std::unique_ptr<OcppOperation> makeOcppOperation(OcppMessage *msg){
    if (msg == nullptr) {
        Serial.print(F("[SimpleOcppOperationFactory] in makeOcppOperation(webSocket, ocppMessage): ocppMessage is null!\n"));
        return nullptr;
    }
    auto operation = makeOcppOperation();
    operation->setOcppMessage(std::unique_ptr<OcppMessage>(msg));
    return operation;
}

std::unique_ptr<OcppOperation> makeOcppOperation(){
    auto result = std::unique_ptr<OcppOperation>(new OcppOperation());
    return result;
}

} //end namespace ArduinoOcpp
