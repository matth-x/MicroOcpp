// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef METERINGSERVICE_H
#define METERINGSERVICE_H

//#define METER_VALUE_SAMPLE_INTERVAL 60 //in seconds

//#define METER_VALUES_SAMPLED_DATA_MAX_LENGTH 4 //after 4 measurements, send the values to the CS

#include <functional>

#include <Variants.h>

#include <ArduinoOcpp/Tasks/Metering/ConnectorMeterValuesRecorder.h>

namespace ArduinoOcpp {

using PowerSampler = std::function<float()>;
using EnergySampler = std::function<float()>;

class OcppEngine;

class MeteringService {
private:
    OcppEngine& context;
    
    const int numConnectors;
    ConnectorMeterValuesRecorder **connectors;
public:
    MeteringService(OcppEngine& context, int numConnectors);

    ~MeteringService();

    void loop();

    void setPowerSampler(int connectorId, PowerSampler powerSampler);

    void setEnergySampler(int connectorId, EnergySampler energySampler);

    float readEnergyActiveImportRegister(int connectorId);
};

} //end namespace ArduinoOcpp
#endif
