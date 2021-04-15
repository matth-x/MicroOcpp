// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef METERINGSERVICE_H
#define METERINGSERVICE_H

//#define METER_VALUE_SAMPLE_INTERVAL 60 //in seconds

//#define METER_VALUES_SAMPLED_DATA_MAX_LENGTH 4 //after 4 measurements, send the values to the CS

#include <LinkedList.h>
#include <WebSocketsClient.h>
#include <ArduinoOcpp/TimeHelper.h>
#include <EEPROMLayout.h>
#include <Variants.h>

#include <ArduinoOcpp/Tasks/Metering/ConnectorMeterValuesRecorder.h>

namespace ArduinoOcpp {

//typedef float (*PowerSampler)();
//typedef float (*EnergySampler)();

typedef std::function<float()> PowerSampler;
typedef std::function<float()> EnergySampler;


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

} //end namespace ArduinoOcpp
#endif
