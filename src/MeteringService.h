// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#ifndef METERINGSERVICE_H
#define METERINGSERVICE_H

#define METER_VALUE_SAMPLE_INTERVAL 60 //in seconds

#define METER_VALUES_SAMPLED_DATA_MAX_LENGTH 4 //after 4 measurements, send the values to the CS

#include <LinkedList.h>
#include <WebSocketsClient.h>
#include "TimeHelper.h"
#include "EEPROMLayout.h"

typedef float (*PowerSampler)();
typedef float (*EnergySampler)();

#include "Variants.h"
#ifdef MULTIPLE_CONN

#include "ConnectorMeterValuesRecorder.h"

class MeteringService {
private:
  WebSocketsClient *webSocket;
  const int numConnectors;
  ConnectorMeterValuesRecorder **connectors;
public:
  MeteringService(WebSocketsClient *webSocket, int numConnectors);

  ~MeteringService();

  void loop();

  void setPowerSampler(int connectorId, PowerSampler powerSampler);

  void setEnergySampler(int connectorId, EnergySampler energySampler);

  float readEnergyActiveImportRegister(int connectorId);
};

#else

class MeteringService {
private:
  WebSocketsClient *webSocket;
  LinkedList<time_t> powerMeasurementTime;
  LinkedList<float> power;
  time_t lastSampleTime = 0; //0 means not charging right now
  float lastPower;

  LinkedList<time_t> energyMeasurementTime;
  LinkedList<float> energy;

  float incrementEnergyActiveImportRegister(float energy);
 
  float (*powerSampler)() = NULL;
  float (*energySampler)() = NULL;

  void addCurrentPowerDataPoint(float currentPower);
  void addEnergyDataPoint(float energyRegister);
  void addEnergyAndPowerDataPoint(float energyRegister, float power);
public:
  MeteringService(WebSocketsClient *webSocket);
  void loop();

  void setPowerSampler(PowerSampler powerSampler);

  void setEnergySampler(EnergySampler energySampler);

  void flushPowerValues();

  void flushEnergyValues();

  void flushEnergyAndPowerValues();

  float readEnergyActiveImportRegister();
};

#endif
#endif
