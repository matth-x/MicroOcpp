// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/UnlockConnector.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::UnlockConnector;
using MicroOcpp::MemJsonDoc;

UnlockConnector::UnlockConnector(Model& model) : AllocOverrider("v16.Operation.", getOperationType()), model(model) {
  
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

std::unique_ptr<MemJsonDoc> UnlockConnector::createConf() {

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

    auto doc = makeMemJsonDoc(JSON_OBJECT_SIZE(1), getMemoryTag());
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = status;
    return doc;
}
