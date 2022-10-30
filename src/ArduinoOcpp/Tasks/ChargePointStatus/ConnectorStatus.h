// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef CONNECTORSTATUS_H
#define CONNECTORSTATUS_H

#include <ArduinoOcpp/Tasks/ChargePointStatus/OcppEvseState.h>
#include <ArduinoOcpp/Tasks/Transactions/TransactionPrerequisites.h>
#include <ArduinoOcpp/Tasks/Transactions/TransactionProcess.h>
#include <ArduinoOcpp/Core/ConfigurationKeyValue.h>
#include <ArduinoOcpp/Core/PollResult.h>
#include <ArduinoOcpp/MessagesV16/CiStrings.h>

#include <vector>
#include <functional>
#include <memory>

#define AVAILABILITY_OPERATIVE 2
#define AVAILABILITY_INOPERATIVE_SCHEDULED 1
#define AVAILABILITY_INOPERATIVE 0

namespace ArduinoOcpp {

class OcppModel;
class OcppMessage;
class Transaction;

class ConnectorStatus {
private:
    OcppModel& context;
    
    const int connectorId;

    std::shared_ptr<Transaction> transaction;

    std::shared_ptr<Configuration<int>> availability;
    bool rebooting = false; //report connector inoperative and reject new charging sessions

    std::function<bool()> connectorPluggedSampler;
    std::function<bool()> evRequestsEnergySampler;
    std::function<bool()> connectorEnergizedSampler;
    std::vector<std::function<const char *()>> connectorErrorCodeSamplers;
    const char *getErrorCode();

    OcppEvseState currentStatus = OcppEvseState::NOT_SET;
    std::shared_ptr<Configuration<int>> minimumStatusDuration; //in seconds
    OcppEvseState reportedStatus = OcppEvseState::NOT_SET;
    ulong t_statusTransition = 0;

    std::function<PollResult<bool>()> onUnlockConnector;

    std::function<TxEnableState(TxTrigger)> onConnectorLockPollTx;
    std::function<TxEnableState(TxTrigger)> onTxBasedMeterPollTx;

    TransactionProcess txProcess;

    std::shared_ptr<Configuration<int>> connectionTimeOut; //in seconds
    std::shared_ptr<Configuration<const char*>> stopTransactionOnInvalidId;
    std::shared_ptr<Configuration<const char*>> stopTransactionOnEVSideDisconnect;
    std::shared_ptr<Configuration<const char*>> unlockConnectorOnEVSideDisconnect;
    std::shared_ptr<Configuration<const char*>> localAuthorizeOffline;
    std::shared_ptr<Configuration<const char*>> localPreAuthorize;
public:
    ConnectorStatus(OcppModel& context, int connectorId);

    /*
     * Relation Session <-> Transaction
     * Session: the EV user is authorized and the OCPP transaction can start immediately without
     *          further confirmation by the user or the OCPP backend
     * Transaction: started by "StartTransaction" and stopped by "StopTransaction".
     * 
     * A session (between the EV user and the OCPP system) is on of two prerequisites to start a
     * transaction. The other prerequisite is that the EV is properly connected to the EVSE
     * (given by ConnectorPluggedSampler and no error code)
     */
    void beginSession(const char *idTag);
    void endSession(const char *reason = nullptr);
    const char *getSessionIdTag();
    uint16_t getSessionWriteCount();
    bool isTransactionRunning();
    int getTransactionId();
    int getTransactionIdSync();
    std::shared_ptr<Transaction>& getTransaction();
    void releaseTransaction();

    int getAvailability();
    void setAvailability(bool available);
    void setRebooting(bool rebooting);
    void setAuthorizationProvider(std::function<const char *()> authorization);
    void setConnectorPluggedSampler(std::function<bool()> connectorPlugged);
    void setEvRequestsEnergySampler(std::function<bool()> evRequestsEnergy);
    void setConnectorEnergizedSampler(std::function<bool()> connectorEnergized);
    void addConnectorErrorCodeSampler(std::function<const char*()> connectorErrorCode);

    void saveState();

    OcppMessage *loop();

    OcppEvseState inferenceStatus();

    bool ocppPermitsCharge();

    void setOnUnlockConnector(std::function<PollResult<bool>()> unlockConnector);
    std::function<PollResult<bool>()> getOnUnlockConnector();

    void setConnectorLock(std::function<TxEnableState(TxTrigger)> lockConnector);
    void setTxBasedMeterUpdate(std::function<TxEnableState(TxTrigger)> updateTxBasedMeter);
};

} //end namespace ArduinoOcpp
#endif
