// matth-x/ArduinoOcpp
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
    OcppTime *ocppTime;

    bool authorized = false;
    String idTag = String('\0');
    bool transactionRunning = false;
    //int transactionId = -1;
    std::shared_ptr<Configuration<int>> transactionId = NULL;
    int transactionIdSync = -1;
    bool evDrawsEnergy = false;
    bool evseOffersEnergy = false;
    OcppEvseState currentStatus = OcppEvseState::NOT_SET;
    std::function<bool()> onUnlockConnector = NULL;
public:
    ConnectorStatus(int connectorId, OcppTime *ocppTime);

    //boolean requestAuthorization();
    void authorize();
    void authorize(String &idTag);
    void unauthorize();
    String &getIdTag();
    int getTransactionId();
    int getTransactionIdSync();
    uint16_t getTransactionWriteCount();
    void setTransactionId(int id);
    void setTransactionIdSync(int id);
    void boot();
    void startEvDrawsEnergy();
    void stopEvDrawsEnergy();
    void startEnergyOffer();
    void stopEnergyOffer();

    void saveState();
    //void recoverState();

    Ocpp16::StatusNotification *loop();

    OcppEvseState inferenceStatus();

    void setOnUnlockConnector(std::function<bool()> unlockConnector);
    std::function<bool()> getOnUnlockConnector();
};

} //end namespace ArduinoOcpp
#endif
