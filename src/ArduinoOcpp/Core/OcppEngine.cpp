// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>

#include <ArduinoOcpp/Core/OcppSocket.h>
#include <ArduinoOcpp/Core/OcppConnection.h>

#include <ArduinoOcpp/Core/OcppError.h>

#include <Variants.h>

namespace ArduinoOcpp {
namespace OcppEngine {

SmartChargingService *ocppEngine_smartChargingService{nullptr};
ChargePointStatusService *ocppEngine_chargePointStatusService{nullptr};
MeteringService *ocppEngine_meteringService{nullptr};
FirmwareService *ocppEngine_firmwareService{nullptr};
DiagnosticsService *ocppEngine_diagnosticsService{nullptr};
OcppTime *ocppEngine_ocppTime{nullptr};

OcppSocket *ocppSock;
OcppConnection *mConnection;

} //end namespace OcppEngine

using namespace ArduinoOcpp::OcppEngine;

void ocppEngine_initialize(OcppSocket *ocppSocket){
  ocppSock = ocppSocket;
  mConnection = new OcppConnection(ocppSock);
}

void initiateOcppOperation(std::unique_ptr<OcppOperation> o) {
    mConnection->initiateOcppOperation(std::move(o));
}

void ocppEngine_loop() {
  ocppSock->loop();
  mConnection->loop();
}

void setSmartChargingService(SmartChargingService *scs) {
  ocppEngine_smartChargingService = scs;
}

SmartChargingService* getSmartChargingService(){
  if (ocppEngine_smartChargingService == nullptr) {
    Serial.print(F("[OcppEngine] Error: in OcppEngine, there is no smartChargingService set, but it is accessed!\n"));
    //no error catch 
  }
  return ocppEngine_smartChargingService;
}

void setChargePointStatusService(ChargePointStatusService *cpss){
  ocppEngine_chargePointStatusService = cpss;
}

ChargePointStatusService *getChargePointStatusService(){
  if (ocppEngine_chargePointStatusService == nullptr) {
    Serial.print(F("[OcppEngine] Error: in OcppEngine, there is no chargePointStatusService set, but it is accessed!\n"));
    //no error catch 
  }
  return ocppEngine_chargePointStatusService;
}

ConnectorStatus *getConnectorStatus(int connectorId) {
  if (getChargePointStatusService() == nullptr) return nullptr;

  ConnectorStatus *result = getChargePointStatusService()->getConnector(connectorId);
  if (result == nullptr) {
    Serial.print(F("[OcppEngine] Error in getConnectorStatus(): cannot fetch connector with given connectorId!\n"));
    //no error catch 
  }
  return result;
}

void setMeteringSerivce(MeteringService *meteringService) {
  ocppEngine_meteringService = meteringService;
}

MeteringService* getMeteringService() {
  if (ocppEngine_meteringService == nullptr) {
    Serial.print(F("[OcppEngine] Error: in OcppEngine, there is no ocppEngine_meteringService set, but it is accessed!\n"));
    //no error catch 
  }
  return ocppEngine_meteringService;
}

void setFirmwareService(FirmwareService *fwService) {
  ocppEngine_firmwareService = fwService;
}

FirmwareService *getFirmwareService() {
  if (ocppEngine_firmwareService == nullptr) {
    Serial.print(F("[OcppEngine] Error: in OcppEngine, there is no ocppEngine_firmwareService set, but it is accessed!\n"));
    //no error catch 
  }
  return ocppEngine_firmwareService;
}

void setDiagnosticsService(DiagnosticsService *diagnosticsService) {
  ocppEngine_diagnosticsService = diagnosticsService;
}

DiagnosticsService *getDiagnosticsService() {
  if (ocppEngine_diagnosticsService == nullptr) {
    Serial.print(F("[OcppEngine] Error: in OcppEngine, there is no ocppEngine_diagnosticsService set, but it is accessed!\n"));
    //no error catch 
  }
  return ocppEngine_diagnosticsService;
}

void ocppEngine_setOcppTime(OcppTime *ocppTime) {
    ocppEngine_ocppTime = ocppTime;
}

OcppTime *getOcppTime() {
    if (ocppEngine_ocppTime == nullptr) {
        Serial.print(F("[OcppEngine] Error: in OcppEngine, there is no ocppEngine_ocppTime set, but it is accessed!\n"));
        //no error catch 
    }
    return ocppEngine_ocppTime;
}

} //end namespace ArduinoOcpp
