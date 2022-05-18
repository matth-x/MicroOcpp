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

#include <ArduinoOcpp/Tasks/Metering/MeterValue.h>
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
    
    std::vector<std::unique_ptr<MeterValue>> sampledData;
    std::vector<std::unique_ptr<MeterValue>> alignedData;
    std::vector<std::unique_ptr<MeterValue>> stopTxnSampledData;
    std::vector<std::unique_ptr<MeterValue>> stopTxnAlignedData;

    std::unique_ptr<MeterValueBuilder> sampledDataBuilder;
    std::unique_ptr<MeterValueBuilder> alignedDataBuilder;
    std::unique_ptr<MeterValueBuilder> stopTxnSampledDataBuilder;
    std::unique_ptr<MeterValueBuilder> stopTxnAlignedDataBuilder;

    std::shared_ptr<Configuration<const char *>> sampledDataSelect;
    std::shared_ptr<Configuration<const char *>> alignedDataSelect;
    std::shared_ptr<Configuration<const char *>> stopTxnSampledDataSelect;
    std::shared_ptr<Configuration<const char *>> stopTxnAlignedDataSelect;

    ulong lastSampleTime = 0; //0 means not charging right now
    OcppTimestamp nextAlignedTime;
    float lastPower;
    int lastTransactionId = -1;
 
    PowerSampler powerSampler = nullptr;
    EnergySampler energySampler = nullptr;
    std::vector<std::unique_ptr<SampledValueSampler>> samplers;
    int energySamplerIndex {-1};

    std::shared_ptr<Configuration<int>> MeterValueSampleInterval;
    std::shared_ptr<Configuration<int>> MeterValuesSampledDataMaxLength;
    std::shared_ptr<Configuration<int>> StopTxnSampledDataMaxLength;

    std::shared_ptr<Configuration<int>> ClockAlignedDataInterval;
    std::shared_ptr<Configuration<int>> MeterValuesAlignedDataMaxLength;
    std::shared_ptr<Configuration<int>> StopTxnAlignedDataMaxLength;
public:
    ConnectorMeterValuesRecorder(OcppModel& context, int connectorId);

    OcppMessage *loop();

    void setPowerSampler(PowerSampler powerSampler);

    void setEnergySampler(EnergySampler energySampler);

    void addMeterValueSampler(std::unique_ptr<SampledValueSampler> meterValueSampler);

    std::unique_ptr<SampledValue> readEnergyActiveImportRegister();

    OcppMessage *takeTriggeredMeterValues();

    OcppMessage *getStopTransactionData();
};

} //end namespace ArduinoOcpp
#endif
