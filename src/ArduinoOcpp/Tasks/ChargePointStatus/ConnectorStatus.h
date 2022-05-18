// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef CONNECTOR_STATUS
#define CONNECTOR_STATUS

#include <ArduinoOcpp/Tasks/ChargePointStatus/OcppEvseState.h>
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

    std::function<bool()> connectorPluggedSampler {nullptr};
    std::function<bool()> evRequestsEnergySampler {nullptr};
    std::function<bool()> connectorEnergizedSampler {nullptr};
    std::vector<std::function<const char *()>> connectorErrorCodeSamplers;
    const char *getErrorCode();

    OcppEvseState currentStatus = OcppEvseState::NOT_SET;
    std::shared_ptr<Configuration<int>> minimumStatusDuration; //in seconds
    OcppEvseState reportedStatus = OcppEvseState::NOT_SET;
    ulong t_statusTransition = 0;

    //std::function<std::unique_ptr<OcppMessage>()> startTransactionBehavior;
    //std::function<std::unique_ptr<OcppMessage>(const char* stopReason)> stopTransactionBehavior;

    std::function<PollResult<bool>()> onUnlockConnector {nullptr};

    std::shared_ptr<Configuration<const char*>> stopTransactionOnInvalidId;
    std::shared_ptr<Configuration<const char*>> stopTransactionOnEVSideDisconnect;
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
    int getTransactionId();
    int getTransactionIdSync();
    uint16_t getTransactionWriteCount();
    void setTransactionId(int id);
    void setTransactionIdSync(int id);

    int getAvailability();
    void setAvailability(bool available);
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
};

} //end namespace ArduinoOcpp
#endif
