// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef CONNECTOR_STATUS
#define CONNECTOR_STATUS

#include <ArduinoOcpp/Tasks/ChargePointStatus/OcppEvseState.h>
#include <ArduinoOcpp/Core/ConfigurationKeyValue.h>
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

    std::shared_ptr<Configuration<int>> availability {nullptr};

    bool session = false;
    char idTag [IDTAG_LEN_MAX + 1] = {'\0'};
    std::shared_ptr<Configuration<const char *>> sIdTag {nullptr};
    std::shared_ptr<Configuration<int>> transactionId {nullptr};
    int transactionIdSync = -1;

    std::shared_ptr<Configuration<int>> connectionTimeOut {nullptr}; //in seconds
    bool connectionTimeOutListen {false};
    ulong connectionTimeOutTimestamp {0}; //in milliseconds

    std::function<bool()> connectorPluggedSampler {nullptr};
    std::function<bool()> evRequestsEnergySampler {nullptr};
    std::function<bool()> connectorEnergizedSampler {nullptr};
    std::vector<std::function<const char *()>> connectorErrorCodeSamplers;
    const char *getErrorCode();
    OcppEvseState currentStatus = OcppEvseState::NOT_SET;
    std::function<bool()> onUnlockConnector {nullptr};
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
    void endSession();
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

    void setOnUnlockConnector(std::function<bool()> unlockConnector);
    std::function<bool()> getOnUnlockConnector();
};

} //end namespace ArduinoOcpp
#endif
