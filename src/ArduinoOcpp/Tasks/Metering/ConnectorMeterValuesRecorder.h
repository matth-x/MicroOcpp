// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef CONNECTOR_METER_VALUES_RECORDER
#define CONNECTOR_METER_VALUES_RECORDER

//#define METER_VALUE_SAMPLE_INTERVAL 60 //in seconds

//#define METER_VALUES_SAMPLED_DATA_MAX_LENGTH 4 //after 4 measurements, send the values to the CS

#include <functional>

#include <ArduinoOcpp/MessagesV16/MeterValues.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Core/OcppTime.h>

namespace ArduinoOcpp {

//typedef float (*PowerSampler)();
//typedef float (*EnergySampler)();
typedef std::function<float()> PowerSampler;
typedef std::function<float()> EnergySampler;

class ConnectorMeterValuesRecorder {
private:
    const int connectorId;
    OcppTime *ocppTime;

    std::vector<OcppTimestamp> sampleTimestamp;
    std::vector<float> energy;
    std::vector<float> power;
    ulong lastSampleTime = 0; //0 means not charging right now
    float lastPower;
    int lastTransactionId = -1;
 
    PowerSampler powerSampler = NULL;
    EnergySampler energySampler = NULL;

    std::shared_ptr<Configuration<int>> MeterValueSampleInterval = NULL;
    std::shared_ptr<Configuration<int>> MeterValuesSampledDataMaxLength = NULL;

    void takeSample();
    Ocpp16::MeterValues *toMeterValues(); //returns message if connector has captured enough samples
    void clear();
public:
    ConnectorMeterValuesRecorder(int connectorId, OcppTime *ocppTime);

    Ocpp16::MeterValues *loop();

    void setPowerSampler(PowerSampler powerSampler);

    void setEnergySampler(EnergySampler energySampler);

    float readEnergyActiveImportRegister();
};

} //end namespace ArduinoOcpp
#endif