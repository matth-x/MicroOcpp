// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef OCPPENGINE_H
#define OCPPENGINE_H

#include <ArduinoJson.h>

#include <ArduinoOcpp/Core/OcppOperation.h>
#include <ArduinoOcpp/Core/OcppSocket.h>
#include <ArduinoOcpp/Core/OcppTime.h>
#include <ArduinoOcpp/Tasks/SmartCharging/SmartChargingService.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Tasks/Metering/MeteringService.h>
#include <ArduinoOcpp/Tasks/FirmwareManagement/FirmwareService.h>
#include <ArduinoOcpp/Tasks/Diagnostics/DiagnosticsService.h>

namespace ArduinoOcpp {

void ocppEngine_initialize(OcppSocket *ocppSocket);

void initiateOcppOperation(std::unique_ptr<OcppOperation> o);

void ocppEngine_loop();

void setSmartChargingService(SmartChargingService *scs);

SmartChargingService* getSmartChargingService();

void setChargePointStatusService(ChargePointStatusService *cpss);

ChargePointStatusService *getChargePointStatusService();

ConnectorStatus *getConnectorStatus(int connectorId);

void setMeteringSerivce(MeteringService *meteringService);

MeteringService* getMeteringService();

void setFirmwareService(FirmwareService *firmwareService);

FirmwareService *getFirmwareService();

void setDiagnosticsService(DiagnosticsService *diagnosticsService);

DiagnosticsService *getDiagnosticsService();

void ocppEngine_setOcppTime(OcppTime *ocppTime);

OcppTime *getOcppTime();

} //end namespace ArduinoOcpp

#endif
