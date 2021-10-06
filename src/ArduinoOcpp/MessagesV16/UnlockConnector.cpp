// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/MessagesV16/UnlockConnector.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>

#include <Variants.h>

using ArduinoOcpp::Ocpp16::UnlockConnector;

UnlockConnector::UnlockConnector() {
  
}

const char* UnlockConnector::getOcppOperationType(){
    return "UnlockConnector";
}

void UnlockConnector::processReq(JsonObject payload) {
    
    int connectorId = payload["connectorId"] | -1;

    ConnectorStatus *connector = getConnectorStatus(connectorId);
    if (!connector) {
        err = true;
        return;
    }

    std::function<bool()> unlockConnector = connector->getOnUnlockConnector();
    if (unlockConnector != NULL) {
        cbDefined = true;
    } else {
        cbDefined = false;
        return;
    }

    cbUnlockSuccessful = unlockConnector();

    //success
}

DynamicJsonDocument* UnlockConnector::createConf(){
  DynamicJsonDocument* doc = new DynamicJsonDocument(JSON_OBJECT_SIZE(1));
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
