// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef CONNECTOR_METER_VALUES_RECORDER
#define CONNECTOR_METER_VALUES_RECORDER

#include <functional>
#include <memory>
#include <vector>

#include <ArduinoOcpp/Tasks/Metering/MeterValue.h>
#include <ArduinoOcpp/Tasks/Metering/MeterStore.h>
#include <ArduinoOcpp/Tasks/Transactions/Transaction.h>
#include <ArduinoOcpp/Core/ConfigurationKeyValue.h>

namespace ArduinoOcpp {

using PowerSampler = std::function<float()>;
using EnergySampler = std::function<float()>;

class OcppModel;
class OcppMessage;
class Transaction;
class MeterStore;

class ConnectorMeterValuesRecorder {
private:
    OcppModel& context;
    const int connectorId;
    MeterStore& meterStore;
    
    std::vector<std::unique_ptr<MeterValue>> meterData;
    std::shared_ptr<TransactionMeterData> stopTxnData;

    std::unique_ptr<MeterValueBuilder> sampledDataBuilder;
    std::unique_ptr<MeterValueBuilder> alignedDataBuilder;
    std::unique_ptr<MeterValueBuilder> stopTxnSampledDataBuilder;
    std::unique_ptr<MeterValueBuilder> stopTxnAlignedDataBuilder;

    std::shared_ptr<Configuration<const char *>> sampledDataSelect;
    std::shared_ptr<Configuration<const char *>> alignedDataSelect;
    std::shared_ptr<Configuration<const char *>> stopTxnSampledDataSelect;
    std::shared_ptr<Configuration<const char *>> stopTxnAlignedDataSelect;

    unsigned long lastSampleTime = 0; //0 means not charging right now
    OcppTimestamp nextAlignedTime;
    std::shared_ptr<Transaction> transaction;
    bool trackTxRunning = false;
 
    PowerSampler powerSampler = nullptr;
    EnergySampler energySampler = nullptr;
    std::vector<std::unique_ptr<SampledValueSampler>> samplers;
    int energySamplerIndex {-1};

    std::shared_ptr<Configuration<int>> MeterValueSampleInterval;
    std::shared_ptr<Configuration<int>> MeterValueCacheSize;

    std::shared_ptr<Configuration<int>> ClockAlignedDataInterval;

    std::shared_ptr<Configuration<bool>> MeterValuesInTxOnly;
    std::shared_ptr<Configuration<bool>> StopTxnDataCapturePeriodic;
public:
    ConnectorMeterValuesRecorder(OcppModel& context, int connectorId, MeterStore& meterStore);

    OcppMessage *loop();

    void setPowerSampler(PowerSampler powerSampler);

    void setEnergySampler(EnergySampler energySampler);

    void addMeterValueSampler(std::unique_ptr<SampledValueSampler> meterValueSampler);

    std::unique_ptr<SampledValue> readTxEnergyMeter(ReadingContext context);

    OcppMessage *takeTriggeredMeterValues();

    void beginTxMeterData(Transaction *transaction);

    std::shared_ptr<TransactionMeterData> endTxMeterData(Transaction *transaction);

    std::shared_ptr<TransactionMeterData> getStopTxMeterData(Transaction *transaction);

};

} //end namespace ArduinoOcpp
#endif
