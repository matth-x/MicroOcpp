// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

/*
 * Implementation of the UCs E01 - E12
 */

#ifndef MO_TRANSACTIONSERVICE_H
#define MO_TRANSACTIONSERVICE_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Model/Metering/MeterValuesV201.h>
#include <MicroOcpp/Core/RequestQueue.h>
#include <MicroOcpp/Core/Memory.h>

#include <memory>
#include <functional>

#ifndef MO_TXRECORD_SIZE_V201
#define MO_TXRECORD_SIZE_V201 4 //maximum number of tx to hold on flash storage
#endif

namespace MicroOcpp {

class Context;
class FilesystemAdapter;
class Variable;

class TransactionService : public MemoryManaged {
public:

    class Evse : public RequestEmitter, public MemoryManaged {
    private:
        Context& context;
        TransactionService& txService;
        Ocpp201::TransactionStoreEvse& txStore;
        const unsigned int evseId;
        unsigned int txNrCounter = 0;
        std::unique_ptr<Ocpp201::Transaction> transaction;
        Ocpp201::TransactionEventData::ChargingState trackChargingState = Ocpp201::TransactionEventData::ChargingState::UNDEFINED;

        std::function<bool()> connectorPluggedInput;
        std::function<bool()> evReadyInput;
        std::function<bool()> evseReadyInput;

        std::function<bool()> startTxReadyInput;
        std::function<bool()> stopTxReadyInput;

        bool beginTransaction();
        bool endTransaction(Ocpp201::Transaction::StoppedReason stoppedReason, Ocpp201::TransactionEventTriggerReason stopTrigger);

        unsigned int txNrBegin = 0; //oldest (historical) transaction on flash. Has no function, but is useful for error diagnosis
        unsigned int txNrFront = 0; //oldest transaction which is still queued to be sent to the server
        unsigned int txNrEnd = 0; //one position behind newest transaction

        Ocpp201::Transaction *txFront = nullptr;
        std::unique_ptr<Ocpp201::Transaction> txFrontCache; //helper owner for txFront. Empty if txFront == transaction.get()
        std::unique_ptr<Ocpp201::TransactionEventData> txEventFront;
        bool txEventFrontIsRequested = false;

    public:
        Evse(Context& context, TransactionService& txService, Ocpp201::TransactionStoreEvse& txStore, unsigned int evseId);

        void loop();

        void setConnectorPluggedInput(std::function<bool()> connectorPlugged);
        void setEvReadyInput(std::function<bool()> evRequestsEnergy);
        void setEvseReadyInput(std::function<bool()> connectorEnergized);

        bool beginAuthorization(IdToken idToken, bool validateIdToken = true); // authorize by swipe RFID
        bool endAuthorization(IdToken idToken = IdToken(), bool validateIdToken = false); // stop authorization by swipe RFID

        // stop transaction, but neither upon user request nor OCPP server request (e.g. after PowerLoss)
        bool abortTransaction(Ocpp201::Transaction::StoppedReason stoppedReason = Ocpp201::Transaction::StoppedReason::Other, Ocpp201::TransactionEventTriggerReason stopTrigger = Ocpp201::TransactionEventTriggerReason::AbnormalCondition);

        Ocpp201::Transaction *getTransaction();

        bool ocppPermitsCharge();

        unsigned int getFrontRequestOpNr() override;
        std::unique_ptr<Request> fetchFrontRequest() override;

        friend TransactionService;
    };

    // TxStartStopPoint (2.6.4.1)
    enum class TxStartStopPoint : uint8_t {
        ParkingBayOccupancy,
        EVConnected,
        Authorized,
        DataSigned,
        PowerPathClosed,
        EnergyTransfer
    };

private:
    Context& context;
    Ocpp201::TransactionStore txStore;
    Evse *evses [MO_NUM_EVSEID] = {nullptr};

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
    TransactionService(Context& context, std::shared_ptr<FilesystemAdapter> filesystem, unsigned int numEvseIds);
    ~TransactionService();

    void loop();

    Evse *getEvse(unsigned int evseId);

    bool parseTxStartStopPoint(const char *src, Vector<TxStartStopPoint>& dst);
};

} // namespace MicroOcpp

#endif // MO_ENABLE_V201

#endif
