// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/TriggerMessage.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Model/Availability/AvailabilityService.h>
#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlService.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16 || MO_ENABLE_V201

using namespace MicroOcpp;

TriggerMessage::TriggerMessage(Context& context, RemoteControlService& rcService) : MemoryManaged("v16.Operation.", "TriggerMessage"), context(context), rcService(rcService) {

}

const char* TriggerMessage::getOperationType(){
    return "TriggerMessage";
}

void TriggerMessage::processReq(JsonObject payload) {

    if (!payload.containsKey("requestedMessage") || !payload["requestedMessage"].is<const char*>()) {
        errorCode = "FormationViolation";
        return;
    }

    const char *requestedMessage = payload["requestedMessage"];
    int evseId = -1;

    #if MO_ENABLE_V16
    if (context.getOcppVersion() == MO_OCPP_V16) {
        evseId = payload["connectorId"] | -1;
    }
    #endif //MO_ENABLE_V16
    #if MO_ENABLE_V201
    if (context.getOcppVersion() == MO_OCPP_V201) {
        evseId = payload["evse"]["id"] | -1;

        if ((payload["evse"]["connectorId"] | 1) != 1) {
            errorCode = "PropertyConstraintViolation";
            return;
        }
    }
    #endif //MO_ENABLE_V201

    if (evseId >= 0 && (unsigned int)evseId >= context.getModelCommon().getNumEvseId()) {
        errorCode = "PropertyConstraintViolation";
        return;
    }

    MO_DBG_INFO("Execute for message type %s, evseId = %i", requestedMessage, evseId);

    status = rcService.triggerMessage(requestedMessage, evseId);
    if (status == TriggerMessageStatus::ERR_INTERNAL) {
        MO_DBG_ERR("triggerMessage() failed");
        errorCode = "InternalError";
        return;
    }
}

std::unique_ptr<JsonDoc> TriggerMessage::createConf(){
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    const char *statusStr = "Rejected";
    switch (status) {
        case TriggerMessageStatus::Accepted:
            statusStr = "Accepted";
            break;
        case TriggerMessageStatus::Rejected:
            statusStr = "Rejected";
            break;
        case TriggerMessageStatus::NotImplemented:
            statusStr = "NotImplemented";
            break;
        case TriggerMessageStatus::ERR_INTERNAL:
            //dead code
            statusStr = "Rejected";
            break;
    }
    payload["status"] = statusStr;
    return doc;
}

#endif //MO_ENABLE_V16 || MO_ENABLE_V201
