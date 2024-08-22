// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_METERINGSERVICE_H
#define MO_METERINGSERVICE_H

#include <functional>
#include <memory>

#include <MicroOcpp/Model/Metering/MeteringConnector.h>
#include <MicroOcpp/Model/Metering/SampledValue.h>
#include <MicroOcpp/Model/Metering/MeterStore.h>
#include <MicroOcpp/Core/Memory.h>

namespace MicroOcpp {

class Context;
class Request;
class FilesystemAdapter;

class MeteringService : public MemoryManaged {
private:
    Context& context;
    MeterStore meterStore;

    Vector<std::unique_ptr<MeteringConnector>> connectors;
public:
    MeteringService(Context& context, int numConnectors, std::shared_ptr<FilesystemAdapter> filesystem);

    void loop();

    void addMeterValueSampler(int connectorId, std::unique_ptr<SampledValueSampler> meterValueSampler);

    std::unique_ptr<SampledValue> readTxEnergyMeter(int connectorId, ReadingContext reason);

    std::unique_ptr<Request> takeTriggeredMeterValues(int connectorId); //snapshot of all meters now

    void beginTxMeterData(Transaction *transaction);

    std::shared_ptr<TransactionMeterData> endTxMeterData(Transaction *transaction); //use return value to keep data in cache

    void abortTxMeterData(unsigned int connectorId); //call this to free resources if txMeterData record is not ended normally. Does not remove files

    std::shared_ptr<TransactionMeterData> getStopTxMeterData(Transaction *transaction); //prefer endTxMeterData when possible

    bool removeTxMeterData(unsigned int connectorId, unsigned int txNr);

    int getNumConnectors() {return connectors.size();}
};

} //end namespace MicroOcpp

#if MO_ENABLE_V201

namespace MicroOcpp {

class Model;
class Variable;

namespace Ocpp201 {

class MeteringServiceEvse : public MemoryManaged {
private:
    Model& model;
    MeterStore& meterStore;
    const unsigned int evseId;

    std::unique_ptr<MeterValueBuilder> sampledDataTxStartedBuilder;
    std::unique_ptr<MeterValueBuilder> sampledDataTxUpdatedBuilder;
    std::unique_ptr<MeterValueBuilder> sampledDataTxEndedBuilder;
 
    Vector<std::unique_ptr<SampledValueSampler>> samplers;
public:
    MeteringServiceEvse(Model& model, MeterStore& meterStore, unsigned int evseId);

    void addMeterValueSampler(std::unique_ptr<SampledValueSampler> meterValueSampler);

    std::unique_ptr<MeterValue> takeTxStartedMeterValue(ReadingContext context = ReadingContext::TransactionBegin);
    std::unique_ptr<MeterValue> takeTxUpdatedMeterValue(ReadingContext context = ReadingContext::SamplePeriodic);
    std::unique_ptr<MeterValue> takeTxEndedMeterValue(ReadingContext context);

    std::unique_ptr<MeterValue> takeTriggeredMeterValues();

    std::shared_ptr<TransactionMeterData> getTxMeterData(unsigned int txNr);

    bool existsSampler(const char *measurand, size_t len);
};

class MeteringService : public MemoryManaged {
private:
    Model& model;
    MeterStore meterStore;

    MeteringServiceEvse* evses [MO_NUM_EVSE] = {nullptr};
public:
    MeteringService(Model& model, size_t numEvses, std::shared_ptr<FilesystemAdapter> filesystem);
    ~MeteringService();

    MeteringServiceEvse *getEvse(unsigned int evseId);
};

} //namespace Ocpp201
} //namespace MicroOcpp

#endif //MO_ENABLE_V201
#endif
