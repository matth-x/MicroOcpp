// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef METERINGSERVICE_H
#define METERINGSERVICE_H

//#define METER_VALUE_SAMPLE_INTERVAL 60 //in seconds

//#define METER_VALUES_SAMPLED_DATA_MAX_LENGTH 4 //after 4 measurements, send the values to the CS

#include <functional>

#include <Variants.h>

#include <ArduinoOcpp/Tasks/Metering/ConnectorMeterValuesRecorder.h>

namespace ArduinoOcpp {

//typedef float (*PowerSampler)();
//typedef float (*EnergySampler)();

typedef std::function<float()> PowerSampler;
typedef std::function<float()> EnergySampler;


class MeteringService {
private:
  const int numConnectors;
  ConnectorMeterValuesRecorder **connectors;
public:
  MeteringService(int numConnectors, OcppTime *ocppTime);

  ~MeteringService();

  void loop();

  void setPowerSampler(int connectorId, PowerSampler powerSampler);

  void setEnergySampler(int connectorId, EnergySampler energySampler);

  float readEnergyActiveImportRegister(int connectorId);
};

} //end namespace ArduinoOcpp
#endif
