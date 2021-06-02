// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef CHARGEPOINTSTATUSSERVICE_H
#define CHARGEPOINTSTATUSSERVICE_H

#include <ArduinoOcpp/Tasks/ChargePointStatus/ConnectorStatus.h>

namespace ArduinoOcpp {

class ChargePointStatusService {
private:
    const int numConnectors;
    OcppTime *ocppTime;
    ConnectorStatus **connectors;

    bool booted = false;

    boolean authorized = false;
    String idTag = String('\0');

public:
    ChargePointStatusService(int numConnectors, OcppTime *ocppTime);

    ~ChargePointStatusService();
    
    void loop();

    void authorize(String &idTag);
    void authorize();
    void boot();
    bool isBooted();
    String &getUnboundIdTag();
    void invalidateUnboundIdTag();
    boolean existsUnboundAuthorization();
    void bindAuthorization(int connectorId);

    ConnectorStatus *getConnector(int connectorId);
    int getNumConnectors();
};

} //end namespace ArduinoOcpp
#endif
