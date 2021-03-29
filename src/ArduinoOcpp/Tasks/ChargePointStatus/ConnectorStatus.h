// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef CONNECTOR_STATUS
#define CONNECTOR_STATUS

#include <ArduinoOcpp/MessagesV16/StatusNotification.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/OcppEvseState.h>
#include <ArduinoOcpp/Core/Configuration.h>

namespace ArduinoOcpp {

class ConnectorStatus {
private:
    const int connectorId;

    bool authorized = false;
    String idTag = String('\0');
    bool transactionRunning = false;
    //int transactionId = -1;
    std::shared_ptr<Configuration<int>> transactionId = NULL;
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

    void saveState();
    //void recoverState();

    Ocpp16::StatusNotification *loop();

    OcppEvseState inferenceStatus();
};

} //end namespace ArduinoOcpp
#endif
