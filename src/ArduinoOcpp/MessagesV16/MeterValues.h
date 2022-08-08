// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef METERVALUES_H
#define METERVALUES_H

#include <ArduinoOcpp/Core/OcppMessage.h>
#include <ArduinoOcpp/Core/OcppTime.h>

#include <vector>

namespace ArduinoOcpp {

class MeterValue;

namespace Ocpp16 {

class MeterValues : public OcppMessage {
private:
    std::vector<std::unique_ptr<MeterValue>> meterValue;

    int connectorId = 0;

public:
    MeterValues(std::vector<std::unique_ptr<MeterValue>>&& meterValue, int connectorId);

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
