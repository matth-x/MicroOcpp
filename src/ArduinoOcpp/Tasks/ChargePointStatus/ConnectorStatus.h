// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef CONNECTORSTATUS_H
#define CONNECTORSTATUS_H

#include <ArduinoOcpp/Tasks/ChargePointStatus/OcppEvseState.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/TransactionPrerequisites.h>
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

class ConnectorStatus {
private:
    OcppModel& context;
    
    const int connectorId;

    std::shared_ptr<Configuration<int>> availability;
    bool rebooting = false; //report connector inoperative and reject new charging sessions

    bool session = false;
    char idTag [IDTAG_LEN_MAX + 1] = {'\0'};
    bool idTagInvalidated {false}; //if StartTransaction.conf() has status != "Accepted"
    std::shared_ptr<Configuration<const char *>> sIdTag;
    std::shared_ptr<Configuration<int>> transactionId;
    int transactionIdSync = -1;
    char endReason [REASON_LEN_MAX + 1] = {'\0'};

    std::shared_ptr<Configuration<int>> connectionTimeOut; //in seconds
    bool connectionTimeOutListen {false};
    ulong connectionTimeOutTimestamp {0}; //in milliseconds

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

    std::function<TxEnableState(TxCondition)> onConnectorLockPollTx;
    std::function<TxEnableState(TxCondition)> onOcmfMeterPollTx;

    std::vector<std::function<TxCondition()>> txTriggerConditions;
    std::vector<std::function<TxEnableState(TxCondition)>> txEnableSequence;
    TxEnableState txEnable {TxEnableState::Inactive}; // = Result of Trigger and Enable Sequence

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
    void setIdTagInvalidated(); //if StartTransaction.conf() has status != "Accepted"
    const char *getSessionIdTag();
    uint16_t getSessionWriteCount();
    int getTransactionId();
    int getTransactionIdSync();
    uint16_t getTransactionWriteCount();
    void setTransactionId(int id);
    void setTransactionIdSync(int id);

    int getAvailability();
    void setAvailability(bool available);
    void setRebooting(bool rebooting);
    void setAuthorizationProvider(std::function<const char *()> authorization);
    void setConnectorPluggedSampler(std::function<bool()> connectorPlugged);
    void setEvRequestsEnergySampler(std::function<bool()> evRequestsEnergy);
    void setConnectorEnergizedSampler(std::function<bool()> connectorEnergized);
    void addConnectorErrorCodeSampler(std::function<const char*()> connectorErrorCode);

    void saveState();
    //void recoverState();

    OcppMessage *loop();

    OcppEvseState inferenceStatus();

    bool ocppPermitsCharge();

    void setOnUnlockConnector(std::function<PollResult<bool>()> unlockConnector);
    std::function<PollResult<bool>()> getOnUnlockConnector();

    void setConnectorLock(std::function<TxEnableState(TxCondition)> lockConnector);
    void setTxBasedMeterUpdate(std::function<TxEnableState(TxCondition)> updateTxBasedMeter);
};

} //end namespace ArduinoOcpp
#endif
