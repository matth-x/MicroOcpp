// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Operations/DataTransfer.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::DataTransfer;

DataTransfer::DataTransfer(const std::string &msg) {
    this->msg = msg;
}

const char* DataTransfer::getOperationType(){
    return "DataTransfer";
}

std::unique_ptr<DynamicJsonDocument> DataTransfer::createReq() {
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(2) + (msg.length() + 1)));
    JsonObject payload = doc->to<JsonObject>();
    payload["vendorId"] = "CustomVendor";
    payload["data"] = msg;
    return doc;
}

void DataTransfer::processConf(JsonObject payload){
    std::string status = payload["status"] | "Invalid";

    if (status == "Accepted") {
        MO_DBG_DEBUG("Request has been accepted");
    } else {
        MO_DBG_INFO("Request has been denied");
    }
}
