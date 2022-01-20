// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef METERVALUES_H
#define METERVALUES_H

#include <ArduinoOcpp/Core/OcppMessage.h>
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
    MeterValues(const std::vector<OcppTimestamp> *sampleTime, const std::vector<float> *energy, const std::vector<float> *power, int connectorId, int transactionId);

    MeterValues(); //for debugging only. Make this for the server pendant

    ~MeterValues();

    const char* getOcppOperationType();

    std::unique_ptr<DynamicJsonDocument> createReq();

    void processConf(JsonObject payload);

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
