// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/SecurityEventNotification.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Debug.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SECURITY_EVENT

using namespace MicroOcpp;

SecurityEventNotification::SecurityEventNotification(Context& context, const char *type, const Timestamp& timestamp) : MemoryManaged("v201.Operation.", "SecurityEventNotification"), context(context), timestamp(timestamp) {
    auto ret = snprintf(this->type, sizeof(this->type), "%s", type);
    if (ret < 0 || (size_t)ret >= sizeof(this->type)) {
        MO_DBG_ERR("type too long (ignored excess characters)");
    }
}

const char* SecurityEventNotification::getOperationType(){
    return "SecurityEventNotification";
}

std::unique_ptr<JsonDoc> SecurityEventNotification::createReq() {

    auto doc = makeJsonDoc(getMemoryTag(),
                JSON_OBJECT_SIZE(2) +
                MO_JSONDATE_SIZE);

    JsonObject payload = doc->to<JsonObject>();

    payload["type"] = (const char*)type; //force zero-copy

    char timestampStr [MO_JSONDATE_SIZE] = {'\0'};
    if (!context.getClock().toJsonString(timestamp, timestampStr, sizeof(timestampStr))) {
        MO_DBG_ERR("internal error");
        timestampStr[0] = '\0';
    }
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

#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SECURITY_EVENT
