// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef CONNECTOR_METER_VALUES_RECORDER
#define CONNECTOR_METER_VALUES_RECORDER

//#define METER_VALUE_SAMPLE_INTERVAL 60 //in seconds

//#define METER_VALUES_SAMPLED_DATA_MAX_LENGTH 4 //after 4 measurements, send the values to the CS

#include <functional>
#include <memory>
#include <vector>

#include <ArduinoOcpp/Core/ConfigurationKeyValue.h>

namespace ArduinoOcpp {

using PowerSampler = std::function<float()>;
using EnergySampler = std::function<float()>;

class OcppModel;
class OcppTimestamp;
class OcppMessage;

class ConnectorMeterValuesRecorder {
private:
    OcppModel& context;

    const int connectorId;

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
    OcppMessage *toMeterValues();
    void clear();
public:
    ConnectorMeterValuesRecorder(OcppModel& context, int connectorId);

    OcppMessage *loop();

    void setPowerSampler(PowerSampler powerSampler);

    void setEnergySampler(EnergySampler energySampler);

    float readEnergyActiveImportRegister();

    OcppMessage *takeMeterValuesNow();
};

} //end namespace ArduinoOcpp
#endif
