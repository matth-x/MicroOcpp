// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include "Variants.h"

#include "ChargePointStatusService.h"
#include "StatusNotification.h"
#include "OcppEngine.h"
#include "SimpleOcppOperationFactory.h"

#include <string.h>


ChargePointStatusService::ChargePointStatusService(WebSocketsClient *webSocket, int numConn)
      : webSocket(webSocket), numConnectors(numConn) {
  
  connectors = (ConnectorStatus**) malloc(numConn * sizeof(ConnectorStatus*));
  for (int i = 0; i < numConn; i++) {
    connectors[i] = new ConnectorStatus(i);
  }

  setChargePointStatusService(this);
}

ChargePointStatusService::~ChargePointStatusService() {
  for (int i = 0; i < numConnectors; i++) {
    delete connectors[i];
  }
  free(connectors);
}

void ChargePointStatusService::loop() {
  if (!booted) return;
  for (int i = 0; i < numConnectors; i++){
    StatusNotification *statusNotificationMsg = connectors[i]->loop();
    if (statusNotificationMsg != NULL) {
      OcppOperation *statusNotification = makeOcppOperation(webSocket, statusNotificationMsg);
      initiateOcppOperation(statusNotification);
    }
  }
}

ConnectorStatus *ChargePointStatusService::getConnector(int connectorId) {
  if (connectorId < 0 || connectorId >= numConnectors) {
    Serial.print(F("[ChargePointStatusService] Error in getConnector(connectorId): connectorId is out of bounds\n"));
    return NULL;
  }

  return connectors[connectorId];
}

void ChargePointStatusService::authorize(String &idTag){
  this->idTag = String(idTag);
  authorize();
}

void ChargePointStatusService::authorize(){
  if (authorized == true){
    if (DEBUG_OUT) Serial.print(F("[ChargePointStatusService] Warning: authorized twice or didn't unauthorize before\n"));
  }
  authorized = true;
}

void ChargePointStatusService::boot() {
  booted = true;
}

String &ChargePointStatusService::getUnboundIdTag() {
  return idTag;
}

boolean ChargePointStatusService::existsUnboundAuthorization() {
  return authorized;
}

void ChargePointStatusService::bindAuthorization(String &idTag, int connectorId) {
  if (connectorId < 0 || connectorId >= numConnectors) {
    Serial.print(F("[ChargePointStatusService] Error in bindAuthorization(connectorId): connectorId is out of bounds\n"));
    return;
  }
  if (DEBUG_OUT) Serial.print(F("[ChargePointStatusService] Connector "));
  if (DEBUG_OUT) Serial.print(connectorId);
  if (DEBUG_OUT) Serial.print(F(" occupies idTag "));
  if (DEBUG_OUT) Serial.print(idTag);
  if (DEBUG_OUT) Serial.print(F("\n"));

  connectors[connectorId]->authorize(idTag);

  authorized = false;
  idTag = String('\0');
}

int ChargePointStatusService::getNumConnectors() {
  return numConnectors;
}
