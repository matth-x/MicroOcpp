// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/MessagesV16/DataTransfer.h>
#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::Ocpp16::DataTransfer;

DataTransfer::DataTransfer(String &msg) {
    this->msg = String(msg);
}

const char* DataTransfer::getOcppOperationType(){
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
    String status = payload["status"] | "Invalid";

    if (status.equals("Accepted")) {
        AO_DBG_DEBUG("Request has been accepted");
    } else {
        AO_DBG_INFO("Request has been denied");
    }
}
