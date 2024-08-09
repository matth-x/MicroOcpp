// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/DataTransfer.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::DataTransfer;
using MicroOcpp::MemJsonDoc;

DataTransfer::DataTransfer() : AllocOverrider("v16.Operation.", getOperationType()) {

}

DataTransfer::DataTransfer(const MemString &msg) : AllocOverrider("v16.Operation.", getOperationType()), msg{makeMemString(msg.c_str(), getMemoryTag())} {

}

const char* DataTransfer::getOperationType(){
    return "DataTransfer";
}

std::unique_ptr<MemJsonDoc> DataTransfer::createReq() {
    auto doc = makeMemJsonDoc(JSON_OBJECT_SIZE(2) + (msg.length() + 1), getMemoryTag());
    JsonObject payload = doc->to<JsonObject>();
    payload["vendorId"] = "CustomVendor";
    payload["data"] = msg;
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
    // Do nothing - we're just required to reject these DataTransfer requests
}

std::unique_ptr<MemJsonDoc> DataTransfer::createConf(){
    auto doc = makeMemJsonDoc(JSON_OBJECT_SIZE(1), getMemoryTag());
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = "Rejected";
    return doc;
}
