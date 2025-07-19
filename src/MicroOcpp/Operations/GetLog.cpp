// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#include <MicroOcpp/Operations/GetLog.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Diagnostics/DiagnosticsService.h>
#include <MicroOcpp/Debug.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_DIAGNOSTICS

using namespace MicroOcpp;

GetLog::GetLog(Context& context, DiagnosticsService& diagService) : MemoryManaged("v16/v201.Operation.", "GetLog"), context(context), diagService(diagService) {

}

void GetLog::processReq(JsonObject payload) {

    auto logType = mo_deserializeLogType(payload["logType"] | "_Undefined");
    if (logType == MO_LogType_UNDEFINED) {
        errorCode = "FormationViolation";
        return;
    }

    int requestId = payload["requestId"] | -1;
    if (requestId < 0) {
        errorCode = "FormationViolation";
        return;
    }

    int retries = payload["retries"] | 0;
    int retryInterval = payload["retryInterval"] | 180;
    if (retries < 0 || retryInterval < 0) {
        errorCode = "PropertyConstraintViolation";
        return;
    }

    JsonObject log = payload["log"];

    const char *remoteLocation = log["remoteLocation"] | "";
    if (!*remoteLocation) {
        errorCode = "FormationViolation";
        return;
    }

    Timestamp oldestTimestamp;
    if (payload.containsKey("oldestTimestamp")) {
        if (!context.getClock().parseString(payload["oldestTimestamp"] | "_Invalid", oldestTimestamp)) {
            errorCode = "FormationViolation";
            MO_DBG_WARN("bad time format");
            return;
        }
    }

    Timestamp latestTimestamp;
    if (payload.containsKey("latestTimestamp")) {
        if (!context.getClock().parseString(payload["latestTimestamp"] | "_Invalid", latestTimestamp)) {
            errorCode = "FormationViolation";
            MO_DBG_WARN("bad time format");
            return;
        }
    }

    status = diagService.getLog(logType, requestId, retries, retryInterval, remoteLocation, oldestTimestamp, latestTimestamp, filename);
}

std::unique_ptr<JsonDoc> GetLog::createConf(){
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(2));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = mo_serializeGetLogStatus(status);
    payload["filename"] = (const char*)filename; //force zero-copy
    return doc;
}

#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_DIAGNOSTICS
