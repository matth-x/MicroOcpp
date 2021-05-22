// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/Tasks/ChargePointStatus/ConnectorStatus.h>

#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <Variants.h>

using namespace ArduinoOcpp;
using namespace ArduinoOcpp::Ocpp16;

ConnectorStatus::ConnectorStatus(int connectorId) : connectorId(connectorId) {

    //Set default transaction ID in memory
    String key = "OCPP_STATE_TRANSACTION_ID_CONNECTOR_";
    String id = String(connectorId, DEC);
    key += id;
    transactionId = declareConfiguration<int>(key.c_str(), -1, CONFIGURATION_FN, false, false, true, false);
    if (!transactionId) {
        Serial.print(F("[ConnectorStatus] Error! Cannot declare transactionId\n"));
    }
}

OcppEvseState ConnectorStatus::inferenceStatus() {
    /*
    * Handle special case: This is the ConnectorStatus for the whole CP (i.e. connectorId=0) --> only states Available, Unavailable, Faulted are possible
    */
    if (connectorId == 0) {
        return OcppEvseState::Available; //no support for Unavailable or Faulted at the moment
    }

    if (!authorized && !getChargePointStatusService()->existsUnboundAuthorization()) {
        return OcppEvseState::Available;
    } else if (!transactionRunning) {
        return OcppEvseState::Preparing;
    } else {
        //Transaction is currently running
        if (!evDrawsEnergy) {
        return OcppEvseState::SuspendedEV;
        }
        if (!evseOffersEnergy) {
        return OcppEvseState::SuspendedEVSE;
        }
        return OcppEvseState::Charging;
    }
}

StatusNotification *ConnectorStatus::loop() {
    OcppEvseState inferencedStatus = inferenceStatus();
    
    if (inferencedStatus != currentStatus) {
        currentStatus = inferencedStatus;
        if (DEBUG_OUT) Serial.print(F("[OcppEvseStateService] Status changed\n"));

        //fire StatusNotification
        //TODO check for online condition: Only inform CS about status change if CP is online
        //TODO check for too short duration condition: Only inform CS about status change if it lasted for longer than MinimumStatusDuration
        return new StatusNotification(connectorId, currentStatus);
    }

    return NULL;
}

void ConnectorStatus::authorize(String &idTag){
    this->idTag = String(idTag);
    authorize();
}

void ConnectorStatus::authorize(){
    if (authorized == true){
        if (DEBUG_OUT) Serial.print(F("[ConnectorStatus] Warning: authorized twice or didn't unauthorize before\n"));
    }
    authorized = true;
}

String &ConnectorStatus::getIdTag() {
    return idTag;
}

void ConnectorStatus::unauthorize(){
    if (authorized == false){
        if (DEBUG_OUT) Serial.print(F("[OcppEvseStateService] Warning: unauthorized twice or didn't authorize before\n"));
    }
    authorized = false;
    idTag = String('\0');
}

void ConnectorStatus::startTransaction(int transId){
    if (transactionRunning == true){
        if (DEBUG_OUT) Serial.print(F("[OcppEvseStateService] Warning: started transaction twice or didn't stop transaction before\n"));
    }
    *transactionId = transId;
    transactionRunning = true;

    saveState();
}

void ConnectorStatus::stopTransaction(){
    if (transactionRunning == false){
        if (DEBUG_OUT) Serial.print(F("[OcppEvseStateService] Warning: stopped transaction twice or didn't start transaction before\n"));
    }
    transactionRunning = false;
    *transactionId = -1;

    saveState();
}

int ConnectorStatus::getTransactionId() {
    return *transactionId;
}

void ConnectorStatus::startEvDrawsEnergy(){
    if (evDrawsEnergy == true){
        if (DEBUG_OUT) Serial.print(F("[OcppEvseStateService] Warning: startEvDrawsEnergy called twice or didn't call stopEvDrawsEnergy before\n"));
    }
    evDrawsEnergy = true;
}

void ConnectorStatus::stopEvDrawsEnergy(){
    if (evDrawsEnergy == false){
        if (DEBUG_OUT) Serial.print(F("[OcppEvseStateService] Warning: stopEvDrawsEnergy called twice or didn't call startEvDrawsEnergy before\n"));
    }
    evDrawsEnergy = false;
}

void ConnectorStatus::startEnergyOffer(){
    if (evseOffersEnergy == true){
        if (DEBUG_OUT) Serial.print(F("[OcppEvseStateService] Warning: startEnergyOffer called twice or didn't call stopEnergyOffer before\n"));
    }
    evseOffersEnergy = true;
}

void ConnectorStatus::stopEnergyOffer(){
    if (evseOffersEnergy == false){
        if (DEBUG_OUT) Serial.print(F("[OcppEvseStateService] Warning: stopEnergyOffer called twice or didn't call startEnergyOffer before\n"));
    }
    evseOffersEnergy = false;
}

void ConnectorStatus::saveState() {
    configuration_save();
}
