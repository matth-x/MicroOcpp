// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/MessagesV16/CustomMessage.h>

using ArduinoOcpp::Ocpp16::CustomMessage;

CustomMessage::CustomMessage(const char *operationType,
            std::function<std::unique_ptr<DynamicJsonDocument> ()> fn_createReq,
            std::function<void (JsonObject)> fn_processConf,
            std::function<bool (const char*, const char*, JsonObject)> fn_processErr,
            std::function<void (StoredOperationHandler*)> fn_initiate,
            std::function<bool (StoredOperationHandler*)> fn_restore) :
        operationType{operationType},
        fn_initiate{fn_initiate},
        fn_restore{fn_restore},
        fn_createReq{fn_createReq},
        fn_processConf{fn_processConf},
        fn_processErr{fn_processErr} {
    
}

CustomMessage::CustomMessage(const char *operationType,
            std::function<void (JsonObject)> fn_processReq,
            std::function<std::unique_ptr<DynamicJsonDocument> ()> fn_createConf,
            std::function<const char* ()> fn_getErrorCode,
            std::function<const char* ()> fn_getErrorDescription,
            std::function<std::unique_ptr<DynamicJsonDocument> ()> fn_getErrorDetails) :
        operationType{operationType},
        fn_processReq{fn_processReq},
        fn_createConf{fn_createConf},
        fn_getErrorCode{fn_getErrorCode},
        fn_getErrorDescription{fn_getErrorDescription},
        fn_getErrorDetails{fn_getErrorDetails} {
    
}

CustomMessage::~CustomMessage() {

}

const char* CustomMessage::getOperationType() {
    return operationType.c_str();
}

void CustomMessage::initiate(StoredOperationHandler *opStore) {
    if (fn_initiate) {
        fn_initiate(opStore);
    }
}

bool CustomMessage::restore(StoredOperationHandler *opStore) {
    if (fn_restore) {
        return fn_restore(opStore);
    } else {
        return false;
    }
}

std::unique_ptr<DynamicJsonDocument> CustomMessage::createReq() {
    return fn_createReq();
}

void CustomMessage::processConf(JsonObject payload) {
    return fn_processConf(payload);
}

void CustomMessage::processReq(JsonObject payload) {
    return fn_processReq(payload);
}

std::unique_ptr<DynamicJsonDocument> CustomMessage::createConf() {
    return fn_createConf();
}

const char *CustomMessage::getErrorCode() {
    if (fn_getErrorCode) {
        return fn_getErrorCode();
    } else {
        return nullptr;
    }
}

const char *CustomMessage::getErrorDescription() {
    if (fn_getErrorDescription) {
        return fn_getErrorDescription();
    } else {
        return "";
    }
}

std::unique_ptr<DynamicJsonDocument> CustomMessage::getErrorDetails() {
    if (fn_getErrorDetails) {
        return fn_getErrorDetails();
    } else {
        return createEmptyDocument();
    }
}
