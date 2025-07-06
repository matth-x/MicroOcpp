// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/Reset.h>
#include <MicroOcpp/Model/Reset/ResetService.h>
#include <MicroOcpp/Model/Common/EvseId.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16

using namespace MicroOcpp;

v16::Reset::Reset(ResetService& resetService) : MemoryManaged("v16.Operation.", "Reset"), resetService(resetService) {

}

const char* v16::Reset::getOperationType(){
    return "Reset";
}

void v16::Reset::processReq(JsonObject payload) {
    /*
     * Process the application data here. Note: you have to implement the device reset procedure in your client code. You have to set
     * a onSendConfListener in which you initiate a reset (e.g. calling ESP.reset() )
     */
    bool isHard = !strcmp(payload["type"] | "undefined", "Hard");

    if (!resetService.isExecuteResetDefined()) {
        MO_DBG_ERR("No reset handler set. Abort operation");
        resetAccepted = false;
    } else {
        if (!resetService.isPreResetDefined() || resetService.notifyReset(isHard) || isHard) {
            resetAccepted = true;
            resetService.initiateReset(isHard);
        }
    }
}

std::unique_ptr<JsonDoc> v16::Reset::createConf() {
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = resetAccepted ? "Accepted" : "Rejected";
    return doc;
}

#endif //MO_ENABLE_V16

#if MO_ENABLE_V201

using namespace MicroOcpp;

v201::Reset::Reset(ResetService& resetService) : MemoryManaged("v201.Operation.", "Reset"), resetService(resetService) {

}

const char* v201::Reset::getOperationType(){
    return "Reset";
}

void v201::Reset::processReq(JsonObject payload) {

    MO_ResetType type;
    const char *typeCstr = payload["type"] | "_Undefined";

    if (!strcmp(typeCstr, "Immediate")) {
        type = MO_ResetType_Immediate;
    } else if (!strcmp(typeCstr, "OnIdle")) {
        type = MO_ResetType_OnIdle;
    } else {
        errorCode = "FormationViolation";
        return;
    }

    int evseIdRaw = payload["evseId"] | 0;

    if (evseIdRaw < 0 || evseIdRaw >= MO_NUM_EVSEID) {
        errorCode = "PropertyConstraintViolation";
        return;
    }

    unsigned int evseId = (unsigned int) evseIdRaw;

    status = resetService.initiateReset(type, evseId);
}

std::unique_ptr<JsonDoc> v201::Reset::createConf() {
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();

    const char *statusCstr = "";
    switch (status) {
        case ResetStatus_Accepted:
            statusCstr = "Accepted";
            break;
        case ResetStatus_Rejected:
            statusCstr = "Rejected";
            break;
        case ResetStatus_Scheduled:
            statusCstr = "Scheduled";
            break;
        default:
            MO_DBG_ERR("internal error");
            break;
    }
    payload["status"] = statusCstr;
    return doc;
}

#endif //MO_ENABLE_V201
