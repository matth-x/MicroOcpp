// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include "Variants.h"

#include <ArduinoOcpp/MessagesV16/RemoteStopTransaction.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>

using ArduinoOcpp::Ocpp16::RemoteStopTransaction;

RemoteStopTransaction::RemoteStopTransaction() {
  
}

const char* RemoteStopTransaction::getOcppOperationType(){
    return "RemoteStopTransaction";
}

void RemoteStopTransaction::processReq(JsonObject payload) {
  transactionId = payload["transactionId"] | -1;
}

DynamicJsonDocument* RemoteStopTransaction::createConf(){
  DynamicJsonDocument* doc = new DynamicJsonDocument(JSON_OBJECT_SIZE(1));
  JsonObject payload = doc->to<JsonObject>();
  
  bool canStopTransaction = false;

  if (getChargePointStatusService() != NULL) {
    for (int i = 0; i < getChargePointStatusService()->getNumConnectors(); i++) {
      ConnectorStatus *connIter = getConnectorStatus(i);
      if (connIter->getTransactionId() == transactionId) {
        canStopTransaction = true; 
      }
    }
  }

  if (canStopTransaction){
    payload["status"] = "Accepted";
  } else {
    payload["status"] = "Rejected";
  }
  
  return doc;
}
