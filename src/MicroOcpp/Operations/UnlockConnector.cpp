// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Operations/UnlockConnector.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::UnlockConnector;

#define MO_UNLOCK_TIMEOUT 10000

UnlockConnector::UnlockConnector(Model& model) : model(model) {
  
}

const char* UnlockConnector::getOperationType(){
    return "UnlockConnector";
}

void UnlockConnector::processReq(JsonObject payload) {
    
    auto connectorId = payload["connectorId"] | -1;

    auto connector = model.getConnector(connectorId);

    if (!connector) {
        err = true;
        return;
    }

    connector->endTransaction(nullptr, "UnlockCommand");
    connector->updateTxNotification(TxNotification::RemoteStop);

    unlockConnector = connector->getOnUnlockConnector();
    if (unlockConnector != nullptr) {
        cbUnlockResult = unlockConnector();
    } else {
        MO_DBG_WARN("Unlock CB undefined");
    }

    timerStart = mocpp_tick_ms();
}

std::unique_ptr<DynamicJsonDocument> UnlockConnector::createConf() {
    if (!err && mocpp_tick_ms() - timerStart < MO_UNLOCK_TIMEOUT) {
        //do poll and if more time is needed, delay creation of conf msg

        if (unlockConnector) {
            if (!cbUnlockResult) {
                cbUnlockResult = unlockConnector();
                if (!cbUnlockResult) {
                    return nullptr; //no result yet - delay confirmation response
                }
            }
        }
    }

    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    if (err || !unlockConnector) {
        payload["status"] = "NotSupported";
    } else if (cbUnlockResult && cbUnlockResult.toValue()) {
        payload["status"] = "Unlocked";
    } else {
        payload["status"] = "UnlockFailed";
    }
    return doc;
}
