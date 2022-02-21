// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef METERINGSERVICE_H
#define METERINGSERVICE_H

#include <functional>

#include <ArduinoOcpp/Tasks/Metering/ConnectorMeterValuesRecorder.h>

namespace ArduinoOcpp {

using PowerSampler = std::function<float()>;  //in Watts (W)
using EnergySampler = std::function<float()>; //in Watt-hours (Wh)

class OcppEngine;
class OcppOperation;

class MeteringService {
private:
    OcppEngine& context;

    std::vector<std::unique_ptr<ConnectorMeterValuesRecorder>> connectors;
public:
    MeteringService(OcppEngine& context, int numConnectors);

    void loop();

    void setPowerSampler(int connectorId, PowerSampler powerSampler);

    void setEnergySampler(int connectorId, EnergySampler energySampler);

    float readEnergyActiveImportRegister(int connectorId);

    std::unique_ptr<OcppOperation> retrieveMeterValues(int connectorId); //returns all recorded MeterValues and deletes own records
};

} //end namespace ArduinoOcpp
#endif
