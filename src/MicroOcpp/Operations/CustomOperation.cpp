// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/CustomOperation.h>

using MicroOcpp::Ocpp16::CustomOperation;
using MicroOcpp::JsonDoc;

CustomOperation::CustomOperation(const char *operationType,
            std::function<std::unique_ptr<JsonDoc> ()> fn_createReq,
            std::function<void (JsonObject)> fn_processConf,
            std::function<bool (const char*, const char*, JsonObject)> fn_processErr) :
        MemoryManaged("v16.Operation.", operationType),
        operationType{makeString(getMemoryTag(), operationType)},
        fn_createReq{fn_createReq},
        fn_processConf{fn_processConf},
        fn_processErr{fn_processErr} {
    
}

CustomOperation::CustomOperation(const char *operationType,
            std::function<void (JsonObject)> fn_processReq,
            std::function<std::unique_ptr<JsonDoc> ()> fn_createConf,
            std::function<const char* ()> fn_getErrorCode,
            std::function<const char* ()> fn_getErrorDescription,
            std::function<std::unique_ptr<JsonDoc> ()> fn_getErrorDetails) :
        MemoryManaged("v16.Operation.", operationType),
        operationType{makeString(getMemoryTag(), operationType)},
        fn_processReq{fn_processReq},
        fn_createConf{fn_createConf},
        fn_getErrorCode{fn_getErrorCode},
        fn_getErrorDescription{fn_getErrorDescription},
        fn_getErrorDetails{fn_getErrorDetails} {
    
}

CustomOperation::~CustomOperation() {

}

const char* CustomOperation::getOperationType() {
    return operationType.c_str();
}

std::unique_ptr<JsonDoc> CustomOperation::createReq() {
    return fn_createReq();
}

void CustomOperation::processConf(JsonObject payload) {
    return fn_processConf(payload);
}

bool CustomOperation::processErr(const char *code, const char *description, JsonObject details) {
    if (fn_processErr) {
        return fn_processErr(code, description, details);
    }
    return true;
}

void CustomOperation::processReq(JsonObject payload) {
    return fn_processReq(payload);
}

std::unique_ptr<JsonDoc> CustomOperation::createConf() {
    return fn_createConf();
}

const char *CustomOperation::getErrorCode() {
    if (fn_getErrorCode) {
        return fn_getErrorCode();
    } else {
        return nullptr;
    }
}

const char *CustomOperation::getErrorDescription() {
    if (fn_getErrorDescription) {
        return fn_getErrorDescription();
    } else {
        return "";
    }
}

std::unique_ptr<JsonDoc> CustomOperation::getErrorDetails() {
    if (fn_getErrorDetails) {
        return fn_getErrorDetails();
    } else {
        return createEmptyDocument();
    }
}
