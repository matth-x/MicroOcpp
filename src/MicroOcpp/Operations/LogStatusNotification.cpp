// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#include <MicroOcpp/Operations/LogStatusNotification.h>
#include <MicroOcpp/Debug.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_DIAGNOSTICS

using namespace MicroOcpp;

LogStatusNotification::LogStatusNotification(MO_UploadLogStatus status, int requestId)
        : MemoryManaged("v16/v201.Operation.", "LogStatusNotification"), status(status), requestId(requestId) {

}

const char* LogStatusNotification::getOperationType(){
    return "LogStatusNotification";
}

std::unique_ptr<JsonDoc> LogStatusNotification::createReq() {
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(2));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = mo_serializeUploadLogStatus(status);
    payload["requestId"] = requestId;
    return doc;
}

void LogStatusNotification::processConf(JsonObject payload) {
    /*
    * Empty payload
    */
}

/*
 * For debugging only
 */
void LogStatusNotification::processReq(JsonObject payload) {

}

/*
 * For debugging only
 */
std::unique_ptr<JsonDoc> LogStatusNotification::createConf(){
    return createEmptyDocument();
}

#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_DIAGNOSTICS
