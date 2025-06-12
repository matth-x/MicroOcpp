// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/GetDiagnostics.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Diagnostics/DiagnosticsService.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16 && MO_ENABLE_DIAGNOSTICS

using namespace MicroOcpp;
using namespace MicroOcpp::Ocpp16;

GetDiagnostics::GetDiagnostics(Context& context, DiagnosticsService& diagService) : MemoryManaged("v16.Operation.", "GetDiagnostics"), context(context), diagService(diagService) {

}

void GetDiagnostics::processReq(JsonObject payload) {

    const char *location = payload["location"] | "";
    //check location URL. Maybe introduce Same-Origin-Policy?
    if (!*location) {
        errorCode = "FormationViolation";
        return;
    }
    
    int retries = payload["retries"] | 1;
    int retryInterval = payload["retryInterval"] | 180;
    if (retries < 0 || retryInterval < 0) {
        errorCode = "PropertyConstraintViolation";
        return;
    }

    Timestamp startTime;
    if (payload.containsKey("startTime")) {
        if (!context.getClock().parseString(payload["startTime"] | "_Invalid", startTime)) {
            errorCode = "FormationViolation";
            MO_DBG_WARN("bad time format");
            return;
        }
    }

    Timestamp stopTime;
    if (payload.containsKey("stopTime")) {
        if (!context.getClock().parseString(payload["stopTime"] | "_Invalid", stopTime)) {
            errorCode = "FormationViolation";
            MO_DBG_WARN("bad time format");
            return;
        }
    }

    (void)diagService.requestDiagnosticsUpload(location, (unsigned int) retries, (unsigned int) retryInterval, startTime, stopTime, filename);
}

std::unique_ptr<JsonDoc> GetDiagnostics::createConf(){
    if (!*filename) {
        return createEmptyDocument();
    } else {
        auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
        JsonObject payload = doc->to<JsonObject>();
        payload["fileName"] = (const char*)filename; //force zero-copy
        return doc;
    }
}

#endif //MO_ENABLE_V16 && MO_ENABLE_DIAGNOSTICS
