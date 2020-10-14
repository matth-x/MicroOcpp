// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#ifndef CONNECTOR_STATUS
#define CONNECTOR_STATUS

#include "StatusNotification.h"
#include "OcppEvseState.h"

class ConnectorStatus {
private:
    const int connectorId;

    bool authorized = false;
    String idTag = String('\0');
    bool transactionRunning = false;
    int transactionId = -1;
    bool evDrawsEnergy = false;
    bool evseOffersEnergy = false;
    OcppEvseState currentStatus = OcppEvseState::NOT_SET;
public:
    ConnectorStatus(int connectorId);

    //boolean requestAuthorization();
    void authorize();
    void authorize(String &idTag);
    void unauthorize();
    String &getIdTag();
    void startTransaction(int transactionId);
    void stopTransaction();
    int getTransactionId();
    void boot();
    void startEvDrawsEnergy();
    void stopEvDrawsEnergy();
    void startEnergyOffer();
    void stopEnergyOffer();

    StatusNotification *loop();

    OcppEvseState inferenceStatus();
};

#endif
