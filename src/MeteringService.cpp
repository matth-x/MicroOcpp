// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include "MeteringService.h"
#include "OcppOperation.h"
#include "MeterValues.h"
#include "OcppEngine.h"
#include "SimpleOcppOperationFactory.h"

#include <EEPROM.h>

MeteringService::MeteringService(WebSocketsClient *webSocket, int numConn)
      : webSocket(webSocket), numConnectors(numConn) {
  
  connectors = (ConnectorMeterValuesRecorder**) malloc(numConn * sizeof(ConnectorMeterValuesRecorder*));
  for (int i = 0; i < numConn; i++) {
    connectors[i] = new ConnectorMeterValuesRecorder(i);
  }

  setMeteringSerivce(this); //make MeteringService available through Ocpp Engine
}

MeteringService::~MeteringService() {
  for (int i = 0; i < numConnectors; i++) {
    delete connectors[i];
  }
  free(connectors);
}

void MeteringService::loop(){

  for (int i = 0; i < numConnectors; i++){
    MeterValues *meterValuesMsg = connectors[i]->loop();
    if (meterValuesMsg != NULL) {
      OcppOperation *meterValues = makeOcppOperation(webSocket, meterValuesMsg);
      meterValues->setTimeout(new FixedTimeout(120000));
      initiateOcppOperation(meterValues);
    }
  }
}

void MeteringService::setPowerSampler(int connectorId, PowerSampler ps){
  if (connectorId < 0 || connectorId >= numConnectors) {
    Serial.print(F("[MeteringService] Error in setPowerSampler(): connectorId is out of bounds\n"));
    return;
  }
  connectors[connectorId]->setPowerSampler(ps);
}

void MeteringService::setEnergySampler(int connectorId, EnergySampler es){
  if (connectorId < 0 || connectorId >= numConnectors) {
    Serial.print(F("[MeteringService] Error in setEnergySampler(): connectorId is out of bounds\n"));
    return;
  }
  connectors[connectorId]->setEnergySampler(es);
}

float MeteringService::readEnergyActiveImportRegister(int connectorId) {
  if (connectorId < 0 || connectorId >= numConnectors) {
    Serial.print(F("[MeteringService] Error in readEnergyActiveImportRegister(): connectorId is out of bounds\n"));
    return 0.f;
  }
  return connectors[connectorId]->readEnergyActiveImportRegister();
}
