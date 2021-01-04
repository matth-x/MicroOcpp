// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#ifndef METERINGSERVICE_H
#define METERINGSERVICE_H

//#define METER_VALUE_SAMPLE_INTERVAL 60 //in seconds

//#define METER_VALUES_SAMPLED_DATA_MAX_LENGTH 4 //after 4 measurements, send the values to the CS

#include <LinkedList.h>
#include <WebSocketsClient.h>
#include "TimeHelper.h"
#include "EEPROMLayout.h"

typedef float (*PowerSampler)();
typedef float (*EnergySampler)();

#include "Variants.h"

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

#endif
