// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef CHARGEPOINTSTATUSSERVICE_H
#define CHARGEPOINTSTATUSSERVICE_H

#include <ArduinoOcpp/Tasks/ChargePointStatus/ConnectorStatus.h>

#include <vector>

namespace ArduinoOcpp {

class OcppEngine;

class ChargePointStatusService {
private:
    OcppEngine& context;
    
    std::vector<std::unique_ptr<ConnectorStatus>> connectors;

    bool booted = false;

    std::string cpCredentials;

public:
    ChargePointStatusService(OcppEngine& context, unsigned int numConnectors);

    ~ChargePointStatusService();
    
    void loop();

    void boot();
    bool isBooted();

    ConnectorStatus *getConnector(int connectorId);
    int getNumConnectors();

    void setChargePointCredentials(DynamicJsonDocument &credentials);
    std::string& getChargePointCredentials();
};

} //end namespace ArduinoOcpp
#endif
