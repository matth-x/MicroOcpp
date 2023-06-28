// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CONNECTOR_H
#define CONNECTOR_H

#include <ArduinoOcpp/Model/ChargeControl/ChargePointStatus.h>
#include <ArduinoOcpp/Model/ChargeControl/ChargePointErrorCode.h>
#include <ArduinoOcpp/Model/ChargeControl/Notification.h>
#include <ArduinoOcpp/Model/Transactions/Transaction.h>
#include <ArduinoOcpp/Core/ConfigurationKeyValue.h>
#include <ArduinoOcpp/Core/PollResult.h>
#include <ArduinoOcpp/Operations/CiStrings.h>

#include <vector>
#include <functional>
#include <memory>

#define AVAILABILITY_OPERATIVE 2
#define AVAILABILITY_INOPERATIVE_SCHEDULED 1
#define AVAILABILITY_INOPERATIVE 0

namespace ArduinoOcpp {

class Context;
class Model;
class Operation;

class Connector {
private:
    Context& context;
    Model& model;
    
    const int connectorId;

    std::shared_ptr<Transaction> transaction;

    std::shared_ptr<Configuration<bool>> availability;
    bool availabilityVolatile = true;

    std::function<bool()> connectorPluggedInput;
    std::function<bool()> evReadyInput;
    std::function<bool()> evseReadyInput;
    std::vector<std::function<ErrorData ()>> errorDataInputs;
    std::vector<bool> trackErrorDataInputs;
    bool isFaulted();
    ChargePointErrorCode getErrorCode();

    ChargePointStatus currentStatus = ChargePointStatus::NOT_SET;
    std::shared_ptr<Configuration<int>> minimumStatusDuration; //in seconds
    ChargePointStatus reportedStatus = ChargePointStatus::NOT_SET;
    unsigned long t_statusTransition = 0;

    std::function<PollResult<bool>()> onUnlockConnector;

    std::function<bool()> startTxReadyInput; //the StartTx request will be delayed while this Input is false
    std::function<bool()> stopTxReadyInput; //the StopTx request will be delayed while this Input is false
    std::function<bool()> occupiedInput; //instead of Available, go into Preparing / Finishing state

    std::function<void(TxNotification,Transaction*)> txNotificationOutput;

    std::shared_ptr<Configuration<int>> connectionTimeOut; //in seconds
    std::shared_ptr<Configuration<bool>> stopTransactionOnInvalidId;
    std::shared_ptr<Configuration<bool>> stopTransactionOnEVSideDisconnect;
    std::shared_ptr<Configuration<bool>> unlockConnectorOnEVSideDisconnect;
    std::shared_ptr<Configuration<bool>> localPreAuthorize;
    std::shared_ptr<Configuration<bool>> allowOfflineTxForUnknownId;

    std::shared_ptr<Configuration<bool>> silentOfflineTransactions;
    std::shared_ptr<Configuration<int>> authorizationTimeout; //in seconds
    std::shared_ptr<Configuration<bool>> freeVendActive;
    std::shared_ptr<Configuration<const char*>> freeVendIdTag;
    bool freeVendTrackPlugged = false;

    bool trackLoopExecute = false; //if loop has been executed once
public:
    Connector(Context& context, int connectorId);
    Connector(Connector&& other) = default;

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
    void endTransaction(const char *idTag = nullptr, StopTxReason reason = StopTxReason::Local);
    
    std::shared_ptr<Transaction>& getTransaction();

    //create detached transaction - won't have any side-effects with the transaction handling of this lib
    std::shared_ptr<Transaction> allocateTransaction(); 

    bool isOperative();
    void setAvailability(bool available);
    void setAvailabilityVolatile(bool available); //set inoperative state but keep only until reboot at most
    void setAuthorizationProvider(std::function<const char *()> authorization);
    void setConnectorPluggedInput(std::function<bool()> connectorPlugged);
    void setEvReadyInput(std::function<bool()> evRequestsEnergy);
    void setEvseReadyInput(std::function<bool()> connectorEnergized);
    void addErrorCodeInput(std::function<ErrorCode ()> connectorErrorCode);
    void addErrorDataInput(std::function<ErrorData ()> errorCodeInput);

    void loop();

    ChargePointStatus getStatus();

    bool ocppPermitsCharge();

    void setOnUnlockConnector(std::function<PollResult<bool>()> unlockConnector);
    std::function<PollResult<bool>()> getOnUnlockConnector();

    void setStartTxReadyInput(std::function<bool()> startTxReady);
    void setStopTxReadyInput(std::function<bool()> stopTxReady);
    void setOccupiedInput(std::function<bool()> occupied);

    void setTxNotificationOutput(std::function<void(TxNotification, Transaction*)> txNotificationOutput);
    void updateTxNotification(TxNotification event);
};

} //end namespace ArduinoOcpp
#endif
