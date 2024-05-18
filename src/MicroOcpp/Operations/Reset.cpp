// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/Reset.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Reset/ResetService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::Reset;

Reset::Reset(Model& model) : model(model) {
  
}

const char* Reset::getOperationType(){
    return "Reset";
}

void Reset::processReq(JsonObject payload) {
    /*
     * Process the application data here. Note: you have to implement the device reset procedure in your client code. You have to set
     * a onSendConfListener in which you initiate a reset (e.g. calling ESP.reset() )
     */
    bool isHard = !strcmp(payload["type"] | "undefined", "Hard");

    if (auto rService = model.getResetService()) {
        if (!rService->getExecuteReset()) {
            MO_DBG_ERR("No reset handler set. Abort operation");
            (void)0;
            //resetAccepted remains false
        } else {
            if (!rService->getPreReset() || rService->getPreReset()(isHard) || isHard) {
                resetAccepted = true;
                rService->initiateReset(isHard);
                for (unsigned int cId = 0; cId < model.getNumConnectors(); cId++) {
                    model.getConnector(cId)->endTransaction(nullptr, isHard ? "HardReset" : "SoftReset");
                }
            }
        }
    } else {
        resetAccepted = true; //assume that onReceiveReset is set
    }
}

std::unique_ptr<DynamicJsonDocument> Reset::createConf() {
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = resetAccepted ? "Accepted" : "Rejected";
    return doc;
}

#if MO_ENABLE_V201

#include <MicroOcpp/Model/ConnectorBase/EvseId.h>

namespace MicroOcpp {
namespace Ocpp201 {

Reset::Reset(ResetService& resetService) : resetService(resetService) {
  
}

const char* Reset::getOperationType(){
    return "Reset";
}

void Reset::processReq(JsonObject payload) {

    ResetType type;
    const char *typeCstr = payload["type"] | "_Undefined";
    
    if (!strcmp(typeCstr, "Immediate")) {
        type = ResetType_Immediate;
    } else if (!strcmp(typeCstr, "OnIdle")) {
        type = ResetType_OnIdle;
    } else {
        errorCode = "FormationViolation";
        return;
    }

    int evseIdRaw = payload["evseId"] | 0;

    if (evseIdRaw < 0 || evseIdRaw >= MO_NUM_EVSE) {
        errorCode = "PropertyConstraintViolation";
        return;
    }

    unsigned int evseId = (unsigned int) evseIdRaw;

    status = resetService.initiateReset(type, evseId);
}

std::unique_ptr<DynamicJsonDocument> Reset::createConf() {
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
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

} //end namespace Ocpp201
} //end namespace MicroOcpp
#endif //MO_ENABLE_V201
