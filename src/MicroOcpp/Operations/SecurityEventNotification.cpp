// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Operations/SecurityEventNotification.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp201::SecurityEventNotification;
using MicroOcpp::JsonDoc;

SecurityEventNotification::SecurityEventNotification(const char *type, const Timestamp& timestamp) : MemoryManaged("v201.Operation.", "SecurityEventNotification"), type(makeString(getMemoryTag(), type ? type : "")), timestamp(timestamp) {

}

const char* SecurityEventNotification::getOperationType(){
    return "SecurityEventNotification";
}

std::unique_ptr<JsonDoc> SecurityEventNotification::createReq() {

    auto doc = makeJsonDoc(getMemoryTag(),
                JSON_OBJECT_SIZE(2) +
                JSONDATE_LENGTH + 1);

    JsonObject payload = doc->to<JsonObject>();

    payload["type"] = type.c_str();

    char timestampStr [JSONDATE_LENGTH + 1];
    timestamp.toJsonString(timestampStr, sizeof(timestampStr));
    payload["timestamp"] = timestampStr;

    return doc;
}

void SecurityEventNotification::processConf(JsonObject) {
    //empty payload
}

void SecurityEventNotification::processReq(JsonObject payload) {
    /**
     * Ignore Contents of this Req-message, because this is for debug purposes only
     */
}

std::unique_ptr<JsonDoc> SecurityEventNotification::createConf() {
    return createEmptyDocument();
}

#endif // MO_ENABLE_V201
