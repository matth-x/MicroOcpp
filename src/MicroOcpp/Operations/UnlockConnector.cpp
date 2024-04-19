// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/UnlockConnector.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::UnlockConnector;

UnlockConnector::UnlockConnector(Model& model) : model(model) {
  
}

const char* UnlockConnector::getOperationType(){
    return "UnlockConnector";
}

void UnlockConnector::processReq(JsonObject payload) {
    
    auto connectorId = payload["connectorId"] | -1;

    auto connector = model.getConnector(connectorId);

    if (!connector) {
        errorCode = "PropertyConstraintViolation";
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

std::unique_ptr<DynamicJsonDocument> UnlockConnector::createConf() {
    if (unlockConnector && mocpp_tick_ms() - timerStart < MO_UNLOCK_TIMEOUT) {
        //do poll and if more time is needed, delay creation of conf msg

        if (cbUnlockResult == UnlockConnectorResult_Pending) {
            cbUnlockResult = unlockConnector();
            if (cbUnlockResult == UnlockConnectorResult_Pending) {
                return nullptr; //no result yet - delay confirmation response
            }
        }
    }

    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    if (!unlockConnector) {
        payload["status"] = "NotSupported";
    } else if (cbUnlockResult == UnlockConnectorResult_Unlocked) {
        payload["status"] = "Unlocked";
    } else {
        payload["status"] = "UnlockFailed";
    }
    return doc;
}
