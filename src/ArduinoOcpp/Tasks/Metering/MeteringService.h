// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef METERINGSERVICE_H
#define METERINGSERVICE_H

#include <functional>
#include <vector>
#include <memory>

#include <ArduinoOcpp/Tasks/Metering/ConnectorMeterValuesRecorder.h>
#include <ArduinoOcpp/Tasks/Metering/SampledValue.h>
#include <ArduinoOcpp/Tasks/Metering/MeterStore.h>

namespace ArduinoOcpp {

using PowerSampler = std::function<float()>;  //in Watts (W)
using EnergySampler = std::function<float()>; //in Watt-hours (Wh)

class OcppEngine;
class OcppOperation;
class FilesystemAdapter;

class MeteringService {
private:
    OcppEngine& context;
    MeterStore meterStore;

    std::vector<std::unique_ptr<ConnectorMeterValuesRecorder>> connectors;
public:
    MeteringService(OcppEngine& context, int numConnectors, std::shared_ptr<FilesystemAdapter> filesystem);

    void loop();

    void setPowerSampler(int connectorId, PowerSampler powerSampler);

    void setEnergySampler(int connectorId, EnergySampler energySampler);

    void addMeterValueSampler(int connectorId, std::unique_ptr<SampledValueSampler> meterValueSampler);

    std::unique_ptr<SampledValue> readTxEnergyMeter(int connectorId, ReadingContext reason);

    std::unique_ptr<OcppOperation> takeTriggeredMeterValues(int connectorId); //snapshot of all meters now

    void beginTxMeterData(Transaction *transaction);

    std::vector<std::unique_ptr<MeterValue>> createStopTxMeterData(Transaction *transaction);

    int getNumConnectors() {return connectors.size();}
};

} //end namespace ArduinoOcpp
#endif
