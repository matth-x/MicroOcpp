#include "Variants.h"

#include "ChargePointStatusService.h"
#include "StatusNotification.h"
#include "OcppEngine.h"
#include "EVSE.h"

#include <string.h>
 
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
    if (DEBUG_APP_LAY) Serial.print(F("[ChargePointStatusService] Status changed\n"));

    //fire StatusNotification
    //TODO check for online condition: Only inform CS about status change if CP is online
    //TODO check for too short duration condition: Only inform CS about status change if it lasted for longer than MinimumStatusDuration
    OcppOperation *statusNotification = new StatusNotification(webSocket, currentStatus);
    initiateOcppOperation(statusNotification);
    //statusNotification->setOnCompleteListener([](JsonDocument *json) {
    // A Callback could be useful here. Dont forget to #include <ArduinoJson.h> when uncommenting
    //});
  }
}

void ChargePointStatusService::authorize(){
  if (authorized == true){
    if (DEBUG_OUTPUT) Serial.print(F("[ChargePointStatusService] Warning: authorized twice or didn't unauthorize before\n"));
  }
  authorized = true;
}

void ChargePointStatusService::unauthorize(){
  if (authorized == false){
    if (DEBUG_OUTPUT) Serial.print(F("[ChargePointStatusService] Warning: unauthorized twice or didn't authorize before\n"));
  }
  authorized = false;
}

void ChargePointStatusService::startTransaction(int transId){
  if (transactionRunning == true){
    if (DEBUG_OUTPUT) Serial.print(F("[ChargePointStatusService] Warning: started transaction twice or didn't stop transaction before\n"));
  }
  transactionId = transId;
  transactionRunning = true;
}

void ChargePointStatusService::boot() {
  //TODO Review: Is it necessary to check in inferenceStatus(), if the CP is booted at all? Problably not ...
}

void ChargePointStatusService::stopTransaction(){
  if (transactionRunning == false){
    if (DEBUG_OUTPUT) Serial.print(F("[ChargePointStatusService] Warning: stopped transaction twice or didn't start transaction before\n"));
  }
  transactionRunning = false;
  transactionId = -1;
}

int ChargePointStatusService::getTransactionId() {
  if (transactionId < 0) {
    if (DEBUG_OUTPUT) Serial.print(F("[ChargePointStatusService] Warning: getTransactionId() returns invalid transactionId. Have you called it after stopTransaction()? (can only be called before) Have you called it before startTransaction?\n"));
  }
  if (transactionRunning == false) {
    if (DEBUG_OUTPUT) Serial.print(F("[ChargePointStatusService] Warning: getTransactionId() called, but there is no transaction running. Have you called it after stopTransaction()? (can only be called before) Have you called it before startTransaction?\n"));
  }
  return transactionId;
}

void ChargePointStatusService::startEvDrawsEnergy(){
  if (evDrawsEnergy == true){
    if (DEBUG_OUTPUT) Serial.print(F("[ChargePointStatusService] Warning: startEvDrawsEnergy called twice or didn't call stopEvDrawsEnergy before\n"));
  }
  evDrawsEnergy = true;
}

void ChargePointStatusService::stopEvDrawsEnergy(){
  if (evDrawsEnergy == false){
    if (DEBUG_OUTPUT) Serial.print(F("[ChargePointStatusService] Warning: stopEvDrawsEnergy called twice or didn't call startEvDrawsEnergy before\n"));
  }
  evDrawsEnergy = false;
}
void ChargePointStatusService::startEnergyOffer(){
  if (evseOffersEnergy == true){
    if (DEBUG_OUTPUT) Serial.print(F("[ChargePointStatusService] Warning: startEnergyOffer called twice or didn't call stopEnergyOffer before\n"));
  }
  evseOffersEnergy = true;
}

void ChargePointStatusService::stopEnergyOffer(){
  if (evseOffersEnergy == false){
    if (DEBUG_OUTPUT) Serial.print(F("[ChargePointStatusService] Warning: stopEnergyOffer called twice or didn't call startEnergyOffer before\n"));
  }
  evseOffersEnergy = false;
}
