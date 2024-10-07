// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/UnlockConnector.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::UnlockConnector;
using MicroOcpp::JsonDoc;

UnlockConnector::UnlockConnector(Model& model) : MemoryManaged("v16.Operation.", "UnlockConnector"), model(model) {
  
}

const char* UnlockConnector::getOperationType(){
    return "UnlockConnector";
}

void UnlockConnector::processReq(JsonObject payload) {

#if MO_ENABLE_CONNECTOR_LOCK
    {
        auto connectorId = payload["connectorId"] | -1;

        auto connector = model.getConnector(connectorId);

        if (!connector) {
            // NotSupported
            return;
        }

        unlockConnector = connector->getOnUnlockConnector();

        if (!unlockConnector) {
            // NotSupported
            return;
        }

        connector->endTransaction(nullptr, "UnlockCommand");
        connector->updateTxNotification(TxNotification::RemoteStop);

        cbUnlockResult = unlockConnector();

        timerStart = mocpp_tick_ms();
    }
#endif //MO_ENABLE_CONNECTOR_LOCK
}

std::unique_ptr<JsonDoc> UnlockConnector::createConf() {

    const char *status = "NotSupported";

#if MO_ENABLE_CONNECTOR_LOCK
    if (unlockConnector) {

        if (mocpp_tick_ms() - timerStart < MO_UNLOCK_TIMEOUT) {
            //do poll and if more time is needed, delay creation of conf msg

            if (cbUnlockResult == UnlockConnectorResult_Pending) {
                cbUnlockResult = unlockConnector();
                if (cbUnlockResult == UnlockConnectorResult_Pending) {
                    return nullptr; //no result yet - delay confirmation response
                }
            }
        }

        if (cbUnlockResult == UnlockConnectorResult_Unlocked) {
            status = "Unlocked";
        } else {
            status = "UnlockFailed";
        }
    }
#endif //MO_ENABLE_CONNECTOR_LOCK

    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = status;
    return doc;
}


#if MO_ENABLE_V201
#if MO_ENABLE_CONNECTOR_LOCK

#include <MicroOcpp/Model/RemoteControl/RemoteControlService.h>

namespace MicroOcpp {
namespace Ocpp201 {

UnlockConnector::UnlockConnector(RemoteControlService& rcService) : MemoryManaged("v201.Operation.UnlockConnector"), rcService(rcService) {

}

const char* UnlockConnector::getOperationType(){
    return "UnlockConnector";
}

void UnlockConnector::processReq(JsonObject payload) {

    int evseId = payload["evseId"] | -1;
    int connectorId = payload["connectorId"] | -1;

    if (evseId < 1 || evseId >= MO_NUM_EVSEID || connectorId < 1) {
        errorCode = "PropertyConstraintViolation";
        return;
    }

    if (connectorId != 1) {
        status = UnlockStatus_UnknownConnector;
        return;
    }

    rcEvse = rcService.getEvse(evseId);
    if (!rcEvse) {
        status = UnlockStatus_UnlockFailed;
        return;
    }

    status = rcEvse->unlockConnector();
    timerStart = mocpp_tick_ms();
}

std::unique_ptr<JsonDoc> UnlockConnector::createConf() {

    if (rcEvse && status == UnlockStatus_PENDING && mocpp_tick_ms() - timerStart < MO_UNLOCK_TIMEOUT) {
        status = rcEvse->unlockConnector();

        if (status == UnlockStatus_PENDING) {
            return nullptr; //no result yet - delay confirmation response
        }
    }

    const char *statusStr = "";
    switch (status) {
        case UnlockStatus_Unlocked:
            statusStr = "Unlocked";
            break;
        case UnlockStatus_UnlockFailed:
            statusStr = "UnlockFailed";
            break;
        case UnlockStatus_OngoingAuthorizedTransaction:
            statusStr = "OngoingAuthorizedTransaction";
            break;
        case UnlockStatus_UnknownConnector:
            statusStr = "UnknownConnector";
            break;
        case UnlockStatus_PENDING:
            MO_DBG_ERR("UnlockConnector timeout");
            statusStr = "UnlockFailed";
            break;
    }

    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = statusStr;
    return doc;
}

} // namespace Ocpp201
} // namespace MicroOcpp

#endif //MO_ENABLE_CONNECTOR_LOCK
#endif //MO_ENABLE_V201
