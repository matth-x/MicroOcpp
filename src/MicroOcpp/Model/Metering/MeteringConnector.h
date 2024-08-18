// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_METERING_CONNECTOR_H
#define MO_METERING_CONNECTOR_H

#include <functional>
#include <memory>

#include <MicroOcpp/Model/Metering/MeterValue.h>
#include <MicroOcpp/Model/Metering/MeterStore.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Core/ConfigurationKeyValue.h>
#include <MicroOcpp/Core/RequestQueue.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Operations/MeterValues.h>

#ifndef MO_METERVALUES_CACHE_MAXSIZE
#define MO_METERVALUES_CACHE_MAXSIZE MO_REQUEST_CACHE_MAXSIZE
#endif

namespace MicroOcpp {

class Context;
class Model;
class Operation;
class Transaction;
class MeterStore;

class MeteringConnector : public MemoryManaged, public RequestEmitter {
private:
    Context& context;
    Model& model;
    const int connectorId;
    MeterStore& meterStore;
    
    Vector<std::unique_ptr<Ocpp16::MeterValues>> meterData;
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
public:
    MeteringConnector(Context& context, int connectorId, MeterStore& meterStore);

    void loop();

    void addMeterValueSampler(std::unique_ptr<SampledValueSampler> meterValueSampler);

    std::unique_ptr<SampledValue> readTxEnergyMeter(ReadingContext model);

    std::unique_ptr<Operation> takeTriggeredMeterValues();

    void beginTxMeterData(Transaction *transaction);

    std::shared_ptr<TransactionMeterData> endTxMeterData(Transaction *transaction);

    void abortTxMeterData();

    std::shared_ptr<TransactionMeterData> getStopTxMeterData(Transaction *transaction);

    bool existsSampler(const char *measurand, size_t len);

    //RequestEmitter implementation
    unsigned int getFrontRequestOpNr() override;
    std::unique_ptr<Request> fetchFrontRequest() override;

};

} //end namespace MicroOcpp
#endif
