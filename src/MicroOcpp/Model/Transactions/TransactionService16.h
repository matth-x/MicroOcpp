// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CONNECTOR_H
#define MO_CONNECTOR_H

#include <MicroOcpp/Core/RequestQueue.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Model/Common/EvseId.h>
#include <MicroOcpp/Model/Configuration/ConfigurationDefs.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Version.h>

#include <memory>

#if MO_ENABLE_V16

#ifndef MO_TXRECORD_SIZE
#define MO_TXRECORD_SIZE 4 //no. of tx to hold on flash storage
#endif

#ifndef MO_REPORT_NOERROR
#define MO_REPORT_NOERROR 0
#endif

namespace MicroOcpp {

class Context;
class Clock;
class Connection;
class Operation;

namespace Ocpp16 {

class Model;
class Configuration;
class MeteringServiceEvse;
class TransactionService;
class AvailabilityServiceEvse;

class TransactionServiceEvse : public RequestQueue, public MemoryManaged {
private:
    Context& context;
    Clock& clock;
    Model& model;
    TransactionService& cService;
    const unsigned int evseId;

    Connection *connection = nullptr;
    MO_FilesystemAdapter *filesystem = nullptr;
    MeteringServiceEvse *meteringServiceEvse = nullptr;
    AvailabilityServiceEvse *availServiceEvse = nullptr;

    Transaction *transaction = nullptr; //shares ownership with transactionFront

    bool (*connectorPluggedInput)(unsigned int evseId, void *userData) = nullptr;
    void  *connectorPluggedInputUserData = nullptr;
    bool (*evReadyInput)(unsigned int evseId, void *userData) = nullptr;
    void  *evReadyInputUserData = nullptr;

    bool (*startTxReadyInput)(unsigned int evseId, void *userData) = nullptr; //the StartTx request will be delayed while this Input is false
    void  *startTxReadyInputUserData = nullptr;
    bool (*stopTxReadyInput)(unsigned int evseId, void *userData) = nullptr; //the StopTx request will be delayed while this Input is false
    void  *stopTxReadyInputUserData = nullptr;

    void (*txNotificationOutput)(MO_TxNotification, unsigned int evseId, void *userData) = nullptr;
    void  *txNotificationOutputUserData = nullptr;

    bool freeVendTrackPlugged = false;

    bool trackLoopExecute = false; //if loop has been executed once

    unsigned int txNrBegin = 0; //oldest (historical) transaction on flash. Has no function, but is useful for error diagnosis
    unsigned int txNrFront = 0; //oldest transaction which is still queued to be sent to the server
    unsigned int txNrEnd = 0; //one position behind newest transaction

    Transaction *transactionFront = nullptr; //shares ownership with transaction
    bool startTransactionInProgress = false;
    bool stopTransactionInProgress = false;

    bool commitTransaction(Transaction *transaction);
public:
    TransactionServiceEvse(Context& context, TransactionService& conService, unsigned int evseId);
    ~TransactionServiceEvse();

    bool setup();

    /*
     * beginTransaction begins the transaction process which eventually leads to a StartTransaction
     * request in the normal case.
     * 
     * Returns true if the transaction process begins successfully
     * Returns false if no transaction process begins due to this call (e.g. other transaction still running)
     */
    bool beginTransaction(const char *idTag);
    bool beginTransaction_authorized(const char *idTag, const char *parentIdTag = nullptr);

    /*
     * End the current transaction process, if existing and not ended yet. This eventually leads to
     * a StopTransaction request, if the transaction process has actually ended due to this call. It
     * is safe to call this function at any time even if no transaction is running
     */
    bool endTransaction(const char *idTag = nullptr, const char *reason = nullptr);
    bool endTransaction_authorized(const char *idTag = nullptr, const char *reason = nullptr);

    Transaction *getTransaction();

    Transaction *allocateTransaction(); 

    void setConnectorPluggedInput(bool (*connectorPlugged)(unsigned int, void*), void *userData);
    void setEvReadyInput(bool (*evReady)(unsigned int, void*), void *userData);
    void setEvseReadyInput(bool (*evseReady)(unsigned int, void*), void *userData);

    void loop();

    bool ocppPermitsCharge();

    void setStartTxReadyInput(bool (*startTxReady)(unsigned int, void*), void *userData);
    void setStopTxReadyInput(bool (*stopTxReady)(unsigned int, void*), void *userData);

    void setTxNotificationOutput(void (*txNotificationOutput)(MO_TxNotification, unsigned int, void*), void *userData);
    void updateTxNotification(MO_TxNotification event);

    unsigned int getFrontRequestOpNr() override;
    std::unique_ptr<Request> fetchFrontRequest() override;

    unsigned int getTxNrBeginHistory(); //if getTxNrBeginHistory() != getTxNrFront(), then return value is the txNr of the oldest tx history entry. If equal to getTxNrFront(), then the history is empty
    unsigned int getTxNrFront(); //if getTxNrEnd() != getTxNrFront(), then return value is the txNr of the oldest transaction queued to be sent to the server. If equal to getTxNrEnd(), then there is no tx to be sent to the server
    unsigned int getTxNrEnd(); //upper limit for the range of valid txNrs

    Transaction *getTransactionFront();
};

class TransactionService : public MemoryManaged {
private:
    Context& context;

    TransactionServiceEvse* evses [MO_NUM_EVSEID] = {nullptr};
    unsigned int numEvseId = MO_NUM_EVSEID;

    Configuration *connectionTimeOutInt = nullptr; //in seconds
    Configuration *stopTransactionOnInvalidIdBool = nullptr;
    Configuration *stopTransactionOnEVSideDisconnectBool = nullptr;
    Configuration *localPreAuthorizeBool = nullptr;
    Configuration *localAuthorizeOfflineBool = nullptr;
    Configuration *allowOfflineTxForUnknownIdBool = nullptr;

    Configuration *silentOfflineTransactionsBool = nullptr;
    Configuration *authorizationTimeoutInt = nullptr; //in seconds
    Configuration *freeVendActiveBool = nullptr;
    Configuration *freeVendIdTagString = nullptr;

    Configuration *txStartOnPowerPathClosedBool = nullptr; // this postpones the tx start point to when evReadyInput becomes true

    Configuration *transactionMessageAttemptsInt = nullptr;
    Configuration *transactionMessageRetryIntervalInt = nullptr;
public:
    TransactionService(Context& context);

    ~TransactionService();

    bool setup();

    void loop();

    TransactionServiceEvse *getEvse(unsigned int evseId);

friend class TransactionServiceEvse;
};

} //namespace Ocpp16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16
#endif
