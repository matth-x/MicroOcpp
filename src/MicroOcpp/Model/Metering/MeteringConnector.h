// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef METERING_CONNECTOR_H
#define METERING_CONNECTOR_H

#include <functional>
#include <memory>
#include <vector>

#include <MicroOcpp/Model/Metering/MeterValue.h>
#include <MicroOcpp/Model/Metering/MeterStore.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Core/ConfigurationKeyValue.h>

namespace MicroOcpp {

class Model;
class Operation;
class Transaction;
class MeterStore;

class MeteringConnector {
private:
    Model& model;
    const int connectorId;
    MeterStore& meterStore;
    
    std::vector<std::unique_ptr<MeterValue>> meterData;
    std::shared_ptr<TransactionMeterData> stopTxnData;

    std::unique_ptr<MeterValueBuilder> sampledDataBuilder;
    std::unique_ptr<MeterValueBuilder> alignedDataBuilder;
    std::unique_ptr<MeterValueBuilder> stopTxnSampledDataBuilder;
    std::unique_ptr<MeterValueBuilder> stopTxnAlignedDataBuilder;

    std::shared_ptr<Configuration> sampledDataSelectString;
    std::shared_ptr<Configuration> alignedDataSelectString;
    std::shared_ptr<Configuration> stopTxnSampledDataSelectString;
    std::shared_ptr<Configuration> stopTxnAlignedDataSelectString;

    unsigned long lastSampleTime = 0; //0 means not charging right now
    Timestamp nextAlignedTime;
    std::shared_ptr<Transaction> transaction;
    bool trackTxRunning = false;
 
    std::vector<std::unique_ptr<SampledValueSampler>> samplers;
    int energySamplerIndex {-1};

    std::shared_ptr<Configuration> meterValueSampleIntervalInt;
    std::shared_ptr<Configuration> meterValueCacheSizeInt;

    std::shared_ptr<Configuration> clockAlignedDataIntervalInt;

    std::shared_ptr<Configuration> meterValuesInTxOnlyBool;
    std::shared_ptr<Configuration> stopTxnDataCapturePeriodicBool;
public:
    MeteringConnector(Model& model, int connectorId, MeterStore& meterStore);

    std::unique_ptr<Operation> loop();

    void addMeterValueSampler(std::unique_ptr<SampledValueSampler> meterValueSampler);

    std::unique_ptr<SampledValue> readTxEnergyMeter(ReadingContext model);

    std::unique_ptr<Operation> takeTriggeredMeterValues();

    void beginTxMeterData(Transaction *transaction);

    std::shared_ptr<TransactionMeterData> endTxMeterData(Transaction *transaction);

    std::shared_ptr<TransactionMeterData> getStopTxMeterData(Transaction *transaction);

    bool existsSampler(const char *measurand, size_t len);

};

} //end namespace MicroOcpp
#endif
