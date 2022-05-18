// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/MessagesV16/UnlockConnector.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::Ocpp16::UnlockConnector;

UnlockConnector::UnlockConnector() {
  
}

const char* UnlockConnector::getOcppOperationType(){
    return "UnlockConnector";
}

void UnlockConnector::processReq(JsonObject payload) {
    
    auto connectorId = payload["connectorId"] | -1;

    if (!ocppModel || !ocppModel->getConnectorStatus(connectorId)) {
        err = true;
        return;
    }

    auto connector = ocppModel->getConnectorStatus(connectorId);

    connector->endSession("UnlockCommand");

    unlockConnector = connector->getOnUnlockConnector();
    if (unlockConnector != nullptr) {
        cbUnlockResult = unlockConnector();
    } else {
        AO_DBG_WARN("Unlock CB undefined");
    }
}

std::unique_ptr<DynamicJsonDocument> UnlockConnector::createConf() {
    if (unlockConnector) {
        if (!cbUnlockResult) {
            cbUnlockResult = unlockConnector();
            if (!cbUnlockResult) {
                return nullptr; //no result yet - delay confirmation response
            }
        }
    }

    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    if (err || !unlockConnector) {
        payload["status"] = "NotSupported";
    } else if (cbUnlockResult.toValue()) {
        payload["status"] = "Unlocked";
    } else {
        payload["status"] = "UnlockFailed";
    }
    return doc;
}
