// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/UnlockConnector.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlService.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16

using namespace MicroOcpp;

Ocpp16::UnlockConnector::UnlockConnector(Context& context, RemoteControlService& rcService) : MemoryManaged("v16.Operation.", "UnlockConnector"), context(context), rcService(rcService) {
  
}

const char* Ocpp16::UnlockConnector::getOperationType(){
    return "UnlockConnector";
}

void Ocpp16::UnlockConnector::processReq(JsonObject payload) {

#if MO_ENABLE_CONNECTOR_LOCK
    {
        auto connectorId = payload["connectorId"] | -1;

        rcEvse = rcService.getEvse(connectorId);
        if (!rcEvse) {
            // NotSupported
            status = UnlockStatus::NotSupported;
            return;
        }

        status = rcEvse->unlockConnector16();
        timerStart = context.getClock().getUptime();
    }
#endif //MO_ENABLE_CONNECTOR_LOCK
}

std::unique_ptr<JsonDoc> Ocpp16::UnlockConnector::createConf() {

#if MO_ENABLE_CONNECTOR_LOCK
    {
        int32_t dtTimerStart;
        context.getClock().delta(context.getClock().getUptime(), timerStart, dtTimerStart);
    
        if (rcEvse && status == UnlockStatus::PENDING && dtTimerStart < MO_UNLOCK_TIMEOUT) {
            status = rcEvse->unlockConnector16();
    
            if (status == UnlockStatus::PENDING) {
                return nullptr; //no result yet - delay confirmation response
            }
        }
    }
#else
    status = UnlockStatus::NotSupported;
#endif //MO_ENABLE_CONNECTOR_LOCK

    const char *statusStr = "";
    switch (status) {
        case UnlockStatus::Unlocked:
            statusStr = "Unlocked";
            break;
        case UnlockStatus::UnlockFailed:
            statusStr = "UnlockFailed";
            break;
        case UnlockStatus::NotSupported:
            statusStr = "NotSupported";
            break;
        case UnlockStatus::PENDING:
            MO_DBG_ERR("UnlockConnector timeout");
            statusStr = "UnlockFailed";
            break;
    }

    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = statusStr;
    return doc;
}

#endif //MO_ENABLE_V16

#if MO_ENABLE_V201 && MO_ENABLE_CONNECTOR_LOCK

using namespace MicroOcpp;

Ocpp201::UnlockConnector::UnlockConnector(Context& context, RemoteControlService& rcService) : MemoryManaged("v201.Operation.UnlockConnector"), context(context), rcService(rcService) {

}

const char* Ocpp201::UnlockConnector::getOperationType(){
    return "UnlockConnector";
}

void Ocpp201::UnlockConnector::processReq(JsonObject payload) {

    int evseId = payload["evseId"] | -1;
    int connectorId = payload["connectorId"] | -1;

    if (evseId < 1 || evseId >= MO_NUM_EVSEID || connectorId < 1) {
        errorCode = "PropertyConstraintViolation";
        return;
    }

    if (connectorId != 1) {
        status = UnlockStatus::UnknownConnector;
        return;
    }

    rcEvse = rcService.getEvse(evseId);
    if (!rcEvse) {
        status = UnlockStatus::UnlockFailed;
        return;
    }

    status = rcEvse->unlockConnector201();
    timerStart = context.getClock().getUptime();
}

std::unique_ptr<JsonDoc> Ocpp201::UnlockConnector::createConf() {

    int32_t dtTimerStart;
    context.getClock().delta(context.getClock().getUptime(), timerStart, dtTimerStart);

    if (rcEvse && status == UnlockStatus::PENDING && dtTimerStart < MO_UNLOCK_TIMEOUT) {
        status = rcEvse->unlockConnector201();

        if (status == UnlockStatus::PENDING) {
            return nullptr; //no result yet - delay confirmation response
        }
    }

    const char *statusStr = "";
    switch (status) {
        case UnlockStatus::Unlocked:
            statusStr = "Unlocked";
            break;
        case UnlockStatus::UnlockFailed:
            statusStr = "UnlockFailed";
            break;
        case UnlockStatus::OngoingAuthorizedTransaction:
            statusStr = "OngoingAuthorizedTransaction";
            break;
        case UnlockStatus::UnknownConnector:
            statusStr = "UnknownConnector";
            break;
        case UnlockStatus::PENDING:
            MO_DBG_ERR("UnlockConnector timeout");
            statusStr = "UnlockFailed";
            break;
    }

    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = statusStr;
    return doc;
}

#endif //MO_ENABLE_V201 && MO_ENABLE_CONNECTOR_LOCK
