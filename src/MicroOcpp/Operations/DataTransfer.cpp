// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/DataTransfer.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16

using namespace MicroOcpp;
using namespace MicroOcpp::v16;

DataTransfer::DataTransfer(const char *msg) : MemoryManaged("v16.Operation.", "DataTransfer") {
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
    // Do nothing - we're just required to reject these DataTransfer requests
}

std::unique_ptr<JsonDoc> DataTransfer::createConf(){
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = "Rejected";
    return doc;
}

#endif //MO_ENABLE_V16
