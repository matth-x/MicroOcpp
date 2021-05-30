// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/Tasks/Metering/MeteringService.h>
#include <ArduinoOcpp/Core/OcppOperation.h>
#include <ArduinoOcpp/MessagesV16/MeterValues.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>

#include <EEPROM.h>

using namespace ArduinoOcpp;
using namespace ArduinoOcpp::Ocpp16;

MeteringService::MeteringService(WebSocketsClient *webSocket, int numConn, OcppTime *ocppTime)
      : webSocket(webSocket), numConnectors(numConn) {
  
    connectors = (ConnectorMeterValuesRecorder**) malloc(numConn * sizeof(ConnectorMeterValuesRecorder*));
    for (int i = 0; i < numConn; i++) {
        connectors[i] = new ConnectorMeterValuesRecorder(i, ocppTime);
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
            OcppOperation *meterValues = makeOcppOperation(meterValuesMsg);
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
