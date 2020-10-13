// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#include "Variants.h"

#include "ChargePointStatusService.h"
#include "StatusNotification.h"
#include "OcppEngine.h"
#include "SimpleOcppOperationFactory.h"

#include <string.h>

#ifdef MULTIPLE_CONN

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

#else
 
ChargePointStatusService::ChargePointStatusService(WebSocketsClient *webSocket)
      : webSocket(webSocket){
  setChargePointStatusService(this);
}

ChargePointStatus ChargePointStatusService::inferenceStatus() {
  if (!authorized) {
    return ChargePointStatus::Available;
  } else if (!transactionRunning) {
    return ChargePointStatus::Preparing;
  } else {
    //Transaction is currently running
    if (!evDrawsEnergy) {
      return ChargePointStatus::SuspendedEV;
    }
    if (!evseOffersEnergy) {
      return ChargePointStatus::SuspendedEVSE;
    }
    return ChargePointStatus::Charging;
  }
}

void ChargePointStatusService::loop() {
  ChargePointStatus inferencedStatus = inferenceStatus();
  
  if (inferencedStatus != currentStatus) {
    currentStatus = inferencedStatus;
    if (DEBUG_OUT) Serial.print(F("[ChargePointStatusService] Status changed\n"));

    //fire StatusNotification
    //TODO check for online condition: Only inform CS about status change if CP is online
    //TODO check for too short duration condition: Only inform CS about status change if it lasted for longer than MinimumStatusDuration
    OcppOperation *statusNotification = makeOcppOperation(webSocket,
      new StatusNotification(currentStatus));
    initiateOcppOperation(statusNotification);
  }
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

String &ChargePointStatusService::getIdTag() {
  return idTag;
}

void ChargePointStatusService::unauthorize(){
  if (authorized == false){
    if (DEBUG_OUT) Serial.print(F("[ChargePointStatusService] Warning: unauthorized twice or didn't authorize before\n"));
  }
  authorized = false;
}

void ChargePointStatusService::startTransaction(int transId){
  if (transactionRunning == true){
    if (DEBUG_OUT) Serial.print(F("[ChargePointStatusService] Warning: started transaction twice or didn't stop transaction before\n"));
  }
  transactionId = transId;
  transactionRunning = true;
}

void ChargePointStatusService::boot() {
  //TODO Review: Is it necessary to check in inferenceStatus(), if the CP is booted at all? Problably not ...
}

void ChargePointStatusService::stopTransaction(){
  if (transactionRunning == false){
    if (DEBUG_OUT) Serial.print(F("[ChargePointStatusService] Warning: stopped transaction twice or didn't start transaction before\n"));
  }
  transactionRunning = false;
  transactionId = -1;
}

int ChargePointStatusService::getTransactionId() {
  return transactionId;
}

void ChargePointStatusService::startEvDrawsEnergy(){
  if (evDrawsEnergy == true){
    if (DEBUG_OUT) Serial.print(F("[ChargePointStatusService] Warning: startEvDrawsEnergy called twice or didn't call stopEvDrawsEnergy before\n"));
  }
  evDrawsEnergy = true;
}

void ChargePointStatusService::stopEvDrawsEnergy(){
  if (evDrawsEnergy == false){
    if (DEBUG_OUT) Serial.print(F("[ChargePointStatusService] Warning: stopEvDrawsEnergy called twice or didn't call startEvDrawsEnergy before\n"));
  }
  evDrawsEnergy = false;
}
void ChargePointStatusService::startEnergyOffer(){
  if (evseOffersEnergy == true){
    if (DEBUG_OUT) Serial.print(F("[ChargePointStatusService] Warning: startEnergyOffer called twice or didn't call stopEnergyOffer before\n"));
  }
  evseOffersEnergy = true;
}

void ChargePointStatusService::stopEnergyOffer(){
  if (evseOffersEnergy == false){
    if (DEBUG_OUT) Serial.print(F("[ChargePointStatusService] Warning: stopEnergyOffer called twice or didn't call startEnergyOffer before\n"));
  }
  evseOffersEnergy = false;
}

#endif
