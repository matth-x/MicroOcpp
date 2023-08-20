// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef METERINGSERVICE_H
#define METERINGSERVICE_H

#include <functional>
#include <vector>
#include <memory>

#include <MicroOcpp/Model/Metering/MeteringConnector.h>
#include <MicroOcpp/Model/Metering/SampledValue.h>
#include <MicroOcpp/Model/Metering/MeterStore.h>

namespace MicroOcpp {

class Context;
class Request;
class FilesystemAdapter;

class MeteringService {
private:
    Context& context;
    MeterStore meterStore;

    std::vector<std::unique_ptr<MeteringConnector>> connectors;
public:
    MeteringService(Context& context, int numConnectors, std::shared_ptr<FilesystemAdapter> filesystem);

    void loop();

    void addMeterValueSampler(int connectorId, std::unique_ptr<SampledValueSampler> meterValueSampler);

    std::unique_ptr<SampledValue> readTxEnergyMeter(int connectorId, ReadingContext reason);

    std::unique_ptr<Request> takeTriggeredMeterValues(int connectorId); //snapshot of all meters now

    void beginTxMeterData(Transaction *transaction);

    std::shared_ptr<TransactionMeterData> endTxMeterData(Transaction *transaction); //use return value to keep data in cache

    std::shared_ptr<TransactionMeterData> getStopTxMeterData(Transaction *transaction); //prefer endTxMeterData when possible

    bool removeTxMeterData(unsigned int connectorId, unsigned int txNr);

    int getNumConnectors() {return connectors.size();}
};

} //end namespace MicroOcpp
#endif
