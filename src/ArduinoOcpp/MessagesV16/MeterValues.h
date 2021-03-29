// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef METERVALUES_H
#define METERVALUES_H

#include <ArduinoOcpp/Core/OcppOperation.h>
#include <LinkedList.h>
#include <ArduinoOcpp/TimeHelper.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class MeterValues : public OcppMessage {
private:

  LinkedList<time_t> sampleTime;
  LinkedList<float> power;
  LinkedList<float> energy;

  int connectorId = 0;
  int transactionId = -1;

public:
  MeterValues(LinkedList<time_t> *sampleTime, LinkedList<float> *energy);

  MeterValues(LinkedList<time_t> *sampleTime, LinkedList<float> *energy, LinkedList<float> *power, int connectorId, int transactionId);

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
