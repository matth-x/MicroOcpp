// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_METERINGSERVICE_H
#define MO_METERINGSERVICE_H

#include <functional>
#include <memory>

#include <MicroOcpp/Model/Metering/SampledValue.h>
#include <MicroOcpp/Model/Metering/MeterStore.h>
#include <MicroOcpp/Model/Metering/MeterValue.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Core/ConfigurationKeyValue.h>
#include <MicroOcpp/Core/RequestQueue.h>
#include <MicroOcpp/Core/Memory.h>

namespace MicroOcpp {

class Context;
class Model;
class Operation;
class Request;
class FilesystemAdapter;

class MeteringServiceEvse : public MemoryManaged, public RequestEmitter {
private:
    Context& context;
    Model& model;
    const int connectorId;
    MeterStore& meterStore;
    
    Vector<std::unique_ptr<MeterValue>> meterData;
    std::unique_ptr<MeterValue> meterDataFront;
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
 
    Vector<std::unique_ptr<SampledValueSampler>> samplers;
    int energySamplerIndex {-1};

    std::shared_ptr<Configuration> meterValueSampleIntervalInt;

    std::shared_ptr<Configuration> clockAlignedDataIntervalInt;

    std::shared_ptr<Configuration> meterValuesInTxOnlyBool;
    std::shared_ptr<Configuration> stopTxnDataCapturePeriodicBool;

    std::shared_ptr<Configuration> transactionMessageAttemptsInt;
    std::shared_ptr<Configuration> transactionMessageRetryIntervalInt;
public:
    MeteringServiceEvse(Context& context, int connectorId, MeterStore& meterStore);

    void loop();

    void addMeterValueSampler(std::unique_ptr<SampledValueSampler> meterValueSampler);

    std::unique_ptr<SampledValue> readTxEnergyMeter(ReadingContext model);

    std::unique_ptr<Operation> takeTriggeredMeterValues();

    void beginTxMeterData(Transaction *transaction);

    std::shared_ptr<TransactionMeterData> endTxMeterData(Transaction *transaction);

    void abortTxMeterData();

    std::shared_ptr<TransactionMeterData> getStopTxMeterData(Transaction *transaction);

    bool removeTxMeterData(unsigned int txNr);

    bool existsSampler(const char *measurand, size_t len);

    //RequestEmitter implementation
    unsigned int getFrontRequestOpNr() override;
    std::unique_ptr<Request> fetchFrontRequest() override;

};

class MeteringService : public MemoryManaged {
private:
    Context& context;
    MeterStore meterStore;
public:
    MeteringService(Context& context, int numConnectors, std::shared_ptr<FilesystemAdapter> filesystem);

    bool setup();

    void loop();
};

} //end namespace MicroOcpp
#endif
