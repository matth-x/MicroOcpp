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
#endif
