#ifndef METERINGSERVICE_H
#define METERINGSERVICE_H

#define METER_VALUE_SAMPLE_INTERVAL 5 //in seconds

#define METER_VALUES_SAMPLED_DATA_MAX_LENGTH 4 //after 4 measurements, send the values to the CS

#include <LinkedList.h>
#include <WebSocketsClient.h>
#include "TimeHelper.h"
#include "EEPROMLayout.h"

typedef float (*PowerSampler)();

class MeteringService {
private:
  WebSocketsClient *webSocket;
  LinkedList<time_t> powerMeasurementTime;
  LinkedList<float> power;
  time_t lastPowerMeasurementTime = 0; //0 means not charging right now
  float lastPower;

  float incrementEnergyActiveImportRegister(float energy);
 
  float (*powerSampler)() = NULL;

  void addCurrentPowerDataPoint(float currentPower);
public:
  MeteringService(WebSocketsClient *webSocket);
  void loop();

  void setPowerSampler(PowerSampler powerSampler);

  void flushPowerValues();

  float readEnergyActiveImportRegister();
};

#endif
