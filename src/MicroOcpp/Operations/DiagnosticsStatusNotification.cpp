// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/DiagnosticsStatusNotification.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Diagnostics/DiagnosticsService.h>

#if MO_ENABLE_V16 && MO_ENABLE_DIAGNOSTICS

using namespace MicroOcpp;
using namespace MicroOcpp::Ocpp16;

DiagnosticsStatusNotification::DiagnosticsStatusNotification(DiagnosticsStatus status) : MemoryManaged("v16.Operation.", "DiagnosticsStatusNotification"), status(status) {
    
}

std::unique_ptr<JsonDoc> DiagnosticsStatusNotification::createReq() {
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = serializeDiagnosticsStatus(status);
    return doc;
}

void DiagnosticsStatusNotification::processConf(JsonObject payload){
    // no payload, nothing to do
}

#endif //MO_ENABLE_V16 && MO_ENABLE_DIAGNOSTICS
