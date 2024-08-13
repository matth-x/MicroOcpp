// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/DataTransfer.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::DataTransfer;
using MicroOcpp::JsonDoc;

DataTransfer::DataTransfer() : MemoryManaged("v16.Operation.", "DataTransfer") {

}

DataTransfer::DataTransfer(const String &msg) : MemoryManaged("v16.Operation.", "DataTransfer"), msg{makeString(getMemoryTag(), msg.c_str())} {

}

const char* DataTransfer::getOperationType(){
    return "DataTransfer";
}

std::unique_ptr<JsonDoc> DataTransfer::createReq() {
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(2) + (msg.length() + 1));
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

std::unique_ptr<JsonDoc> DataTransfer::createConf(){
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = "Rejected";
    return doc;
}
