// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/SimpleOcppOperationFactory.h>

#include <ArduinoOcpp/MessagesV16/Authorize.h>
#include <ArduinoOcpp/MessagesV16/BootNotification.h>
#include <ArduinoOcpp/MessagesV16/GetCompositeSchedule.h>
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
#include <ArduinoOcpp/MessagesV16/ClearCache.h>
#include <ArduinoOcpp/MessagesV16/SendLocalList.h>
#include <ArduinoOcpp/MessagesV16/GetLocalListVersion.h>
#include <ArduinoOcpp/MessagesV16/ReserveNow.h>
#include <ArduinoOcpp/MessagesV16/CancelReservation.h>

#include <ArduinoOcpp/Debug.h>

#include <string.h>
#include <algorithm>
#include <vector>

namespace ArduinoOcpp {

class TBDeiniter {
public:
    virtual ~TBDeiniter() = default;
};

std::vector<std::unique_ptr<TBDeiniter>> toBeDeinitialized;

template<class T>
class TBDeiniterConcrete : public TBDeiniter {
private:
    T& obj;
public:
    TBDeiniterConcrete(T& obj) : obj(obj) { }
    ~TBDeiniterConcrete() {obj = nullptr;}
};

template<class T>
void deinit_afterwards(T& obj) {
    toBeDeinitialized.emplace_back(std::unique_ptr<TBDeiniter>{new TBDeiniterConcrete<T>{obj}});
}

OnReceiveReqListener onAuthorizeRequest;
void setOnAuthorizeRequestListener(OnReceiveReqListener listener){
    onAuthorizeRequest = listener;
    deinit_afterwards(onAuthorizeRequest);
}

OnReceiveReqListener onBootNotificationRequest;
void setOnBootNotificationRequestListener(OnReceiveReqListener listener){
    onBootNotificationRequest = listener;
    deinit_afterwards(onBootNotificationRequest);
}

OnReceiveReqListener onSetChargingProfileRequest;
void setOnSetChargingProfileRequestListener(OnReceiveReqListener listener){
    onSetChargingProfileRequest = listener;
    deinit_afterwards(onSetChargingProfileRequest);
}

OnReceiveReqListener onStartTransactionRequest;
void setOnStartTransactionRequestListener(OnReceiveReqListener listener){
    onStartTransactionRequest = listener;
    deinit_afterwards(onStartTransactionRequest);
}

OnReceiveReqListener onTriggerMessageRequest;
void setOnTriggerMessageRequestListener(OnReceiveReqListener listener){
    onTriggerMessageRequest = listener;
    deinit_afterwards(onTriggerMessageRequest);
}

OnReceiveReqListener onRemoteStartTransactionReceiveRequest;
void setOnRemoteStartTransactionReceiveRequestListener(OnReceiveReqListener listener) {
    onRemoteStartTransactionReceiveRequest = listener;
    deinit_afterwards(onRemoteStartTransactionReceiveRequest);
}

OnSendConfListener onRemoteStartTransactionSendConf;
void setOnRemoteStartTransactionSendConfListener(OnSendConfListener listener){
    onRemoteStartTransactionSendConf = listener;
    deinit_afterwards(onRemoteStartTransactionSendConf);
}

OnReceiveReqListener onRemoteStopTransactionReceiveRequest;
void setOnRemoteStopTransactionReceiveRequestListener(OnReceiveReqListener listener){
    onRemoteStopTransactionReceiveRequest = listener;
    deinit_afterwards(onRemoteStopTransactionReceiveRequest);
}

OnSendConfListener onRemoteStopTransactionSendConf;
void setOnRemoteStopTransactionSendConfListener(OnSendConfListener listener){
    onRemoteStopTransactionSendConf = listener;
    deinit_afterwards(onRemoteStopTransactionSendConf);
}

OnSendConfListener onChangeConfigurationReceiveReq;
void setOnChangeConfigurationReceiveRequestListener(OnReceiveReqListener listener){
    onChangeConfigurationReceiveReq = listener;
    deinit_afterwards(onChangeConfigurationReceiveReq);
}

OnSendConfListener onChangeConfigurationSendConf;
void setOnChangeConfigurationSendConfListener(OnSendConfListener listener){
    onChangeConfigurationSendConf = listener;
    deinit_afterwards(onChangeConfigurationSendConf);
}

OnSendConfListener onGetConfigurationReceiveReq;
void setOnGetConfigurationReceiveReqListener(OnSendConfListener listener){
    onGetConfigurationReceiveReq = listener;
    deinit_afterwards(onGetConfigurationReceiveReq);
}

OnSendConfListener onGetConfigurationSendConf;
void setOnGetConfigurationSendConfListener(OnSendConfListener listener){
    onGetConfigurationSendConf = listener;
    deinit_afterwards(onGetConfigurationSendConf);
}

OnSendConfListener onResetReceiveReq;
void setOnResetReceiveRequestListener(OnReceiveReqListener listener) {
    onResetReceiveReq = listener;
    deinit_afterwards(onResetReceiveReq);
}

OnSendConfListener onResetSendConf;
void setOnResetSendConfListener(OnSendConfListener listener){
    onResetSendConf = listener;
    deinit_afterwards(onResetSendConf);
}

OnReceiveReqListener onUpdateFirmwareReceiveReq;
void setOnUpdateFirmwareReceiveRequestListener(OnReceiveReqListener listener) {
    onUpdateFirmwareReceiveReq = listener;
    deinit_afterwards(onUpdateFirmwareReceiveReq);
}

OnReceiveReqListener onMeterValuesReceiveReq;
void setOnMeterValuesReceiveRequestListener(OnReceiveReqListener listener) {
    onMeterValuesReceiveReq = listener;
    deinit_afterwards(onMeterValuesReceiveReq);
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

void simpleOcppFactory_deinitialize() {
    customMessagesRegistry.clear();
    toBeDeinitialized.clear();
}

CustomOcppMessageCreatorEntry *makeCustomOcppMessage(const char *messageType) {
    for (auto it = customMessagesRegistry.begin(); it != customMessagesRegistry.end(); ++it) {
        if (!strcmp(it->messageType, messageType)) {
            return &(*it);
        }
    }
    return nullptr;
}

std::unique_ptr<OcppOperation> makeFromJson(const JsonDocument& json) {
    const char* messageType = json[2];
    return makeOcppOperation(messageType);
}

std::unique_ptr<OcppOperation> makeOcppOperation(const char *messageType, int connectorId) {
    auto operation = makeOcppOperation();
    auto msg = std::unique_ptr<OcppMessage>{nullptr};

    if (CustomOcppMessageCreatorEntry *entry = makeCustomOcppMessage(messageType)) {
        msg = std::unique_ptr<OcppMessage>(entry->creator());
        operation->setOnReceiveReqListener(entry->onReceiveReq);
    } else if (!strcmp(messageType, "Authorize")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::Authorize("A0-00-00-00")); //send default idTag
        operation->setOnReceiveReqListener(onAuthorizeRequest);
    } else if (!strcmp(messageType, "BootNotification")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::BootNotification());
        operation->setOnReceiveReqListener(onBootNotificationRequest);
    } else if (!strcmp(messageType, "GetCompositeSchedule")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::GetCompositeSchedule());
    } else if (!strcmp(messageType, "Heartbeat")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::Heartbeat());
    } else if (!strcmp(messageType, "MeterValues")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::MeterValues());
        operation->setOnReceiveReqListener(onMeterValuesReceiveReq);
    } else if (!strcmp(messageType, "SetChargingProfile")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::SetChargingProfile());
        operation->setOnReceiveReqListener(onSetChargingProfileRequest);
    } else if (!strcmp(messageType, "StatusNotification")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::StatusNotification(connectorId));
    } else if (!strcmp(messageType, "StartTransaction")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::StartTransaction());
        operation->setOnReceiveReqListener(onStartTransactionRequest);
    } else if (!strcmp(messageType, "StopTransaction")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::StopTransaction());
    } else if (!strcmp(messageType, "TriggerMessage")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::TriggerMessage());
        operation->setOnReceiveReqListener(onTriggerMessageRequest);
    } else if (!strcmp(messageType, "RemoteStartTransaction")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::RemoteStartTransaction());
        operation->setOnReceiveReqListener(onRemoteStartTransactionReceiveRequest);
        operation->setOnSendConfListener(onRemoteStartTransactionSendConf);
    } else if (!strcmp(messageType, "RemoteStopTransaction")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::RemoteStopTransaction());
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
        operation->setOnReceiveReqListener(onResetReceiveReq);
        operation->setOnSendConfListener(onResetSendConf);
    } else if (!strcmp(messageType, "UpdateFirmware")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::UpdateFirmware());
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
    } else if (!strcmp(messageType, "ClearCache")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::ClearCache());
    } else if (!strcmp(messageType, "SendLocalList")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::SendLocalList());
    } else if (!strcmp(messageType, "GetLocalListVersion")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::GetLocalListVersion());
    } else if (!strcmp(messageType, "ReserveNow")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::ReserveNow());
    } else if (!strcmp(messageType, "CancelReservation")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::CancelReservation());
    } else {
        AO_DBG_WARN("Operation not supported");
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
        AO_DBG_ERR("msg is null");
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
