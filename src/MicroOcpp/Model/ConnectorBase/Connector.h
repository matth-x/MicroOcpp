// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CONNECTOR_H
#define CONNECTOR_H

#include <MicroOcpp/Model/ConnectorBase/ChargePointStatus.h>
#include <MicroOcpp/Model/ConnectorBase/ChargePointErrorData.h>
#include <MicroOcpp/Model/ConnectorBase/Notification.h>
#include <MicroOcpp/Core/ConfigurationKeyValue.h>
#include <MicroOcpp/Core/PollResult.h>
#include <MicroOcpp/Operations/CiStrings.h>

#include <vector>
#include <functional>
#include <memory>

namespace MicroOcpp {

class Context;
class Model;
class Operation;
class Transaction;

class Connector {
private:
    Context& context;
    Model& model;
    
    const int connectorId;

    std::shared_ptr<Transaction> transaction;

    std::shared_ptr<Configuration> availabilityBool;
    char availabilityBoolKey [sizeof(MO_CONFIG_EXT_PREFIX "AVAIL_CONN_xxxx") + 1];
    bool availabilityVolatile = true;

    std::function<bool()> connectorPluggedInput;
    std::function<bool()> evReadyInput;
    std::function<bool()> evseReadyInput;
    std::vector<std::function<ErrorData ()>> errorDataInputs;
    std::vector<bool> trackErrorDataInputs;
    bool isFaulted();
    const char *getErrorCode();

    ChargePointStatus currentStatus = ChargePointStatus::NOT_SET;
    std::shared_ptr<Configuration> minimumStatusDurationInt; //in seconds
    ChargePointStatus reportedStatus = ChargePointStatus::NOT_SET;
    unsigned long t_statusTransition = 0;

    std::function<PollResult<bool>()> onUnlockConnector;

    std::function<bool()> startTxReadyInput; //the StartTx request will be delayed while this Input is false
    std::function<bool()> stopTxReadyInput; //the StopTx request will be delayed while this Input is false
    std::function<bool()> occupiedInput; //instead of Available, go into Preparing / Finishing state

    std::function<void(Transaction*,TxNotification)> txNotificationOutput;

    std::shared_ptr<Configuration> connectionTimeOutInt; //in seconds
    std::shared_ptr<Configuration> stopTransactionOnInvalidIdBool;
    std::shared_ptr<Configuration> stopTransactionOnEVSideDisconnectBool;
    std::shared_ptr<Configuration> localPreAuthorizeBool;
    std::shared_ptr<Configuration> localAuthorizeOfflineBool;
    std::shared_ptr<Configuration> allowOfflineTxForUnknownIdBool;

    std::shared_ptr<Configuration> silentOfflineTransactionsBool;
    std::shared_ptr<Configuration> authorizationTimeoutInt; //in seconds
    std::shared_ptr<Configuration> freeVendActiveBool;
    std::shared_ptr<Configuration> freeVendIdTagString;
    bool freeVendTrackPlugged = false;

    bool trackLoopExecute = false; //if loop has been executed once
public:
    Connector(Context& context, int connectorId);
    Connector(const Connector&) = delete;
    Connector(Connector&&) = delete;
    Connector& operator=(const Connector&) = delete;

    ~Connector();

    /*
     * beginTransaction begins the transaction process which eventually leads to a StartTransaction
     * request in the normal case.
     * 
     * If the transaction process begins successfully, a Transaction object is returned
     * If no transaction process begins due to this call, nullptr is returned (e.g. memory allocation failed)
     */
    std::shared_ptr<Transaction> beginTransaction(const char *idTag);
    std::shared_ptr<Transaction> beginTransaction_authorized(const char *idTag, const char *parentIdTag = nullptr);

    /*
     * End the current transaction process, if existing and not ended yet. This eventually leads to
     * a StopTransaction request, if the transaction process has actually ended due to this call. It
     * is safe to call this function at any time even if no transaction is running
     */
    void endTransaction(const char *idTag = nullptr, const char *reason = nullptr);
    
    std::shared_ptr<Transaction>& getTransaction();

    //create detached transaction - won't have any side-effects with the transaction handling of this lib
    std::shared_ptr<Transaction> allocateTransaction(); 

    bool isOperative();
    void setAvailability(bool available);
    void setAvailabilityVolatile(bool available); //set inoperative state but keep only until reboot at most
    void setConnectorPluggedInput(std::function<bool()> connectorPlugged);
    void setEvReadyInput(std::function<bool()> evRequestsEnergy);
    void setEvseReadyInput(std::function<bool()> connectorEnergized);
    void addErrorCodeInput(std::function<const char*()> connectorErrorCode);
    void addErrorDataInput(std::function<ErrorData ()> errorCodeInput);

    void loop();

    ChargePointStatus getStatus();

    bool ocppPermitsCharge();

    void setOnUnlockConnector(std::function<PollResult<bool>()> unlockConnector);
    std::function<PollResult<bool>()> getOnUnlockConnector();

    void setStartTxReadyInput(std::function<bool()> startTxReady);
    void setStopTxReadyInput(std::function<bool()> stopTxReady);
    void setOccupiedInput(std::function<bool()> occupied);

    void setTxNotificationOutput(std::function<void(Transaction*,TxNotification)> txNotificationOutput);
    void updateTxNotification(TxNotification event);
};

} //end namespace MicroOcpp
#endif
