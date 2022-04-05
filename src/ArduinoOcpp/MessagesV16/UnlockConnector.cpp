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
    
    int connectorId = payload["connectorId"] | -1;

    if (!ocppModel || !ocppModel->getConnectorStatus(connectorId)) {
        err = true;
        return;
    }

    auto connector = ocppModel->getConnectorStatus(connectorId);

    connector->endSession("UnlockCommand");

    std::function<bool()> unlockConnector = connector->getOnUnlockConnector();
    if (unlockConnector != nullptr) {
        cbDefined = true;
    } else {
        cbDefined = false;
        AO_DBG_WARN("Unlock CB undefined");
        return;
    }

    cbUnlockSuccessful = unlockConnector();
}

std::unique_ptr<DynamicJsonDocument> UnlockConnector::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    if (err || !cbDefined) {
        payload["status"] = "NotSupported";
    } else if (cbUnlockSuccessful) {
        payload["status"] = "Unlocked";
    } else {
        payload["status"] = "UnlockFailed";
    }
    return doc;
}
