// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

/*
 * Implementation of the UCs E01 - E12
 */

#ifndef MO_TRANSACTIONSERVICE_H
#define MO_TRANSACTIONSERVICE_H

#include <MicroOcpp/Version.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlDefs.h>
#include <MicroOcpp/Model/Common/EvseId.h>
#include <MicroOcpp/Core/RequestQueue.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Memory.h>

#include <memory>
#include <functional>

#ifndef MO_TXRECORD_SIZE_V201
#define MO_TXRECORD_SIZE_V201 4 //maximum number of tx to hold on flash storage
#endif

#if MO_ENABLE_V201

namespace MicroOcpp {

class Context;
class Clock;

namespace Ocpp201 {

// TxStartStopPoint (2.6.4.1)
enum class TxStartStopPoint : uint8_t {
    ParkingBayOccupancy,
    EVConnected,
    Authorized,
    DataSigned,
    PowerPathClosed,
    EnergyTransfer
};

class Variable;
class TransactionService;
class MeteringServiceEvse;

class TransactionServiceEvse : public RequestQueue, public MemoryManaged {
private:
    Context& context;
    Clock& clock;
    TransactionService& txService;
    TransactionStoreEvse& txStore;
    MeteringServiceEvse *meteringEvse = nullptr;
    const unsigned int evseId;
    unsigned int txNrCounter = 0;
    std::unique_ptr<Transaction> transaction;
    TransactionEventData::ChargingState trackChargingState = TransactionEventData::ChargingState::UNDEFINED;

    bool (*connectorPluggedInput)(unsigned int evseId, void *userData) = nullptr;
    void  *connectorPluggedInputUserData = nullptr;
    bool (*evReadyInput)(unsigned int evseId, void *userData) = nullptr;
    void  *evReadyInputUserData = nullptr;
    bool (*evseReadyInput)(unsigned int evseId, void *userData) = nullptr;
    void  *evseReadyInputUserData = nullptr;

    bool (*startTxReadyInput)(unsigned int evseId, void *userData) = nullptr; //the StartTx request will be delayed while this Input is false
    void  *startTxReadyInputUserData = nullptr;
    bool (*stopTxReadyInput)(unsigned int evseId, void *userData) = nullptr; //the StopTx request will be delayed while this Input is false
    void  *stopTxReadyInputUserData = nullptr;

    void (*txNotificationOutput)(MO_TxNotification, unsigned int evseId, void *userData) = nullptr;
    void  *txNotificationOutputUserData = nullptr;

    bool beginTransaction();
    bool endTransaction(MO_TxStoppedReason stoppedReason, MO_TxEventTriggerReason stopTrigger);

    TransactionEventData::ChargingState getChargingState();

    unsigned int txNrBegin = 0; //oldest (historical) transaction on flash. Has no function, but is useful for error diagnosis
    unsigned int txNrFront = 0; //oldest transaction which is still queued to be sent to the server
    unsigned int txNrEnd = 0; //one position behind newest transaction

    Transaction *txFront = nullptr;
    std::unique_ptr<Transaction> txFrontCache; //helper owner for txFront. Empty if txFront == transaction.get()
    std::unique_ptr<TransactionEventData> txEventFront;
    bool txEventFrontIsRequested = false;

public:
    TransactionServiceEvse(Context& context, TransactionService& txService, TransactionStoreEvse& txStore, unsigned int evseId);
    ~TransactionServiceEvse();

    bool setup();

    void loop();

    void setConnectorPluggedInput(bool (*connectorPlugged)(unsigned int, void*), void *userData);
    void setEvReadyInput(bool (*evReady)(unsigned int, void*), void *userData);
    void setEvseReadyInput(bool (*evseReady)(unsigned int, void*), void *userData);

    void setStartTxReadyInput(bool (*startTxReady)(unsigned int, void*), void *userData);
    void setStopTxReadyInput(bool (*stopTxReady)(unsigned int, void*), void *userData);

    void setTxNotificationOutput(void (*txNotificationOutput)(MO_TxNotification, unsigned int, void*), void *userData);
    void updateTxNotification(MO_TxNotification event);

    bool beginAuthorization(IdToken idToken, bool validateIdToken = true, IdToken groupIdToken = nullptr); // authorize by swipe RFID, groupIdToken ignored if validateIdToken = true
    bool endAuthorization(IdToken idToken = IdToken(), bool validateIdToken = false, IdToken groupIdToken = nullptr); // stop authorization by swipe RFID, groupIdToken ignored if validateIdToken = true

    // stop transaction, but neither upon user request nor OCPP server request (e.g. after PowerLoss)
    bool abortTransaction(MO_TxStoppedReason stoppedReason = MO_TxStoppedReason_Other, MO_TxEventTriggerReason stopTrigger = MO_TxEventTriggerReason_AbnormalCondition);

    Transaction *getTransaction();

    bool ocppPermitsCharge();

    TriggerMessageStatus triggerTransactionEvent();

    unsigned int getFrontRequestOpNr() override;
    std::unique_ptr<Request> fetchFrontRequest() override;

    friend TransactionService;
};

class TransactionService : public MemoryManaged {
private:
    Context& context;
    TransactionStore txStore;
    TransactionServiceEvse* evses [MO_NUM_EVSEID] = {nullptr};
    unsigned int numEvseId = MO_NUM_EVSEID;

    MO_FilesystemAdapter *filesystem = nullptr;
    uint32_t (*rngCb)() = nullptr;

    Variable *txStartPointString = nullptr;
    Variable *txStopPointString = nullptr;
    Variable *stopTxOnInvalidIdBool = nullptr;
    Variable *stopTxOnEVSideDisconnectBool = nullptr;
    Variable *evConnectionTimeOutInt = nullptr;
    Variable *sampledDataTxUpdatedInterval = nullptr;
    Variable *sampledDataTxEndedInterval = nullptr;
    Variable *messageAttemptsTransactionEventInt = nullptr;
    Variable *messageAttemptIntervalTransactionEventInt = nullptr;
    Variable *silentOfflineTransactionsBool = nullptr;
    uint16_t trackTxStartPoint = -1;
    uint16_t trackTxStopPoint = -1;
    Vector<TxStartStopPoint> txStartPointParsed;
    Vector<TxStartStopPoint> txStopPointParsed;
    bool isTxStartPoint(TxStartStopPoint check);
    bool isTxStopPoint(TxStartStopPoint check);
public:
    TransactionService(Context& context);
    ~TransactionService();

    bool setup();

    void loop();

    TransactionServiceEvse *getEvse(unsigned int evseId);

    bool parseTxStartStopPoint(const char *src, Vector<TxStartStopPoint>& dst);

friend class TransactionServiceEvse;
};

} //namespace Ocpp201
} //namespace MicroOcpp
#endif //MO_ENABLE_V201
#endif
