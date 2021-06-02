// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef METERVALUES_H
#define METERVALUES_H

#include <ArduinoOcpp/Core/OcppOperation.h>
#include <ArduinoOcpp/Core/OcppTime.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class MeterValues : public OcppMessage {
private:

  std::vector<OcppTimestamp> sampleTime;
  std::vector<float> power;
  std::vector<float> energy;

  int connectorId = 0;
  int transactionId = -1;

public:
  MeterValues(std::vector<OcppTimestamp> *sampleTime, std::vector<float> *energy);

  MeterValues(std::vector<OcppTimestamp> *sampleTime, std::vector<float> *energy, std::vector<float> *power, int connectorId, int transactionId);

  MeterValues(); //for debugging only. Make this for the server pendant

  ~MeterValues();

  const char* getOcppOperationType();

  DynamicJsonDocument* createReq();

  void processConf(JsonObject payload);

  void processReq(JsonObject payload);

  DynamicJsonDocument* createConf();
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
