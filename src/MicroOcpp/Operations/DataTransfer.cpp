// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License


#include <ArduinoJson.hpp>
#include <MicroOcpp/Model/DataTransfer/DataTransferService.h>
#include <MicroOcpp/Operations/DataTransfer.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Platform.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16 || MO_ENABLE_V201

using namespace MicroOcpp;

DataTransfer::DataTransfer(Context& context, const char *msg) 
    : MemoryManaged("v16/v201.Operation.", "DataTransfer"), context(context)
{
    if (msg) {
        size_t size = strlen(msg);
        this->msg = static_cast<char*>(MO_MALLOC(getMemoryTag(), size));
        if (this->msg) {
            memset(this->msg, 0, size);
            auto ret = snprintf(this->msg, size, "%s", msg);
            if (ret < 0 || (size_t)ret >= size) {
                MO_DBG_ERR("snprintf: %i", ret);
                MO_FREE(this->msg);
                this->msg = nullptr;
                //send empty string
            }
            //success
        } else {
            MO_DBG_ERR("OOM");
            //send empty string
        }
    }
}

DataTransfer::~DataTransfer() {
    MO_FREE(msg);
    msg = nullptr;
    if (op)
    {
        delete op;
        op = nullptr;
    }
}

const char* DataTransfer::getOperationType(){
    return "DataTransfer";
}

std::unique_ptr<JsonDoc> DataTransfer::createReq() {
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(2));
    JsonObject payload = doc->to<JsonObject>();
    payload["vendorId"] = "CustomVendor";
    payload["data"] = msg ? (const char*)msg : "";
    return doc;
}

void DataTransfer::processConf(JsonObject payload){
    const char *status = payload["status"] | "Invalid";

    if (!strcmp(status, "Accepted")) {
        MO_DBG_DEBUG("Request has been accepted");
    } else {
        MO_DBG_INFO("Request has been denied");
    }
}

void DataTransfer::processReq(JsonObject payload) {
    if (payload.containsKey("vendorId") && payload.containsKey("messageId") && payload.containsKey("data"))
    {
        auto dataTransferService = context.getModel().getDataTransferService();
        if (dataTransferService) {
            op = dataTransferService->getRegisteredOperation(payload["vendorId"], payload["messageId"]);
            if (op) {
                // in ocppv16, data is text
                // in ocppv201, data is anyType
                // for more flexibility, we just pass the whole payload to the registered operation
                op->processReq(payload);
                errorCode = op->getErrorCode();
            }
            else {
                MO_DBG_DEBUG("Failed to find DataTransfer operation for vendorId: %s and messageId: %s", 
                    payload["vendorId"].as<const char*>(), payload["messageId"].as<const char*>());
                errorCode = "NotImplemented";
            }
        }
    }
    else {
        // Op set to nullptr if no operation was found
        op = nullptr;
        errorCode = "FormationViolation";
    }
}

std::unique_ptr<JsonDoc> DataTransfer::createConf() {
    // processReq() should have been called before createConf()
    // if op is not nullptr, it will have itself createConf
    if (op) {
        return op->createConf();
    }
    else {
        auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
        JsonObject payload = doc->to<JsonObject>();
        payload["status"] = "Rejected";
        return doc;
    }
}

#endif //MO_ENABLE_V16 || MO_ENABLE_V201
