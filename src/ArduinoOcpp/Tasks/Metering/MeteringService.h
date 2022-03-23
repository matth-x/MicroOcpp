// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef METERINGSERVICE_H
#define METERINGSERVICE_H

#include <functional>
#include <vector>
#include <memory>

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

    std::unique_ptr<OcppOperation> takeMeterValuesNow(int connectorId); //snapshot of all meters now

    int getNumConnectors() {return connectors.size();}
};

} //end namespace ArduinoOcpp
#endif
