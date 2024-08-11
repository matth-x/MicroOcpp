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
#include <MicroOcpp/Model/Transactions/TransactionDefs.h>
#include <MicroOcpp/Core/Memory.h>

#include <memory>
#include <functional>

namespace MicroOcpp {

class Context;
class Variable;

class TransactionService : public AllocOverrider {
public:

    class Evse : public AllocOverrider {
    private:
        Context& context;
        TransactionService& txService;
        const unsigned int evseId;
        std::shared_ptr<Ocpp201::Transaction> transaction;
        Ocpp201::TransactionEventData::ChargingState trackChargingState = Ocpp201::TransactionEventData::ChargingState::UNDEFINED;

        std::function<bool()> connectorPluggedInput;
        std::function<bool()> evReadyInput;
        std::function<bool()> evseReadyInput;

        std::function<bool()> startTxReadyInput;
        std::function<bool()> stopTxReadyInput;

        std::unique_ptr<Ocpp201::Transaction> allocateTransaction();
    public:
        Evse(Context& context, TransactionService& txService, unsigned int evseId);

        void loop();

        void setConnectorPluggedInput(std::function<bool()> connectorPlugged);
        void setEvReadyInput(std::function<bool()> evRequestsEnergy);
        void setEvseReadyInput(std::function<bool()> connectorEnergized);

        bool beginAuthorization(IdToken idToken, bool validateIdToken = true); // authorize by swipe RFID
        bool endAuthorization(IdToken idToken = IdToken(), bool validateIdToken = true); // stop authorization by swipe RFID

        // stop transaction, but neither upon user request nor OCPP server request (e.g. after PowerLoss)
        bool abortTransaction(Ocpp201::Transaction::StopReason stopReason = Ocpp201::Transaction::StopReason::Other, Ocpp201::TransactionEventTriggerReason stopTrigger = Ocpp201::TransactionEventTriggerReason::AbnormalCondition);

        std::shared_ptr<Ocpp201::Transaction>& getTransaction();

        bool ocppPermitsCharge();

        friend TransactionService;
    };

private:

    // TxStartStopPoint (2.6.4.1)
    enum class TxStartStopPoint : uint8_t {
        ParkingBayOccupancy,
        EVConnected,
        Authorized,
        DataSigned,
        PowerPathClosed,
        EnergyTransfer
    };

    Context& context;
    MemVector<Evse> evses;

    Variable *txStartPointString = nullptr;
    Variable *txStopPointString = nullptr;
    Variable *stopTxOnInvalidIdBool = nullptr;
    Variable *stopTxOnEVSideDisconnectBool = nullptr;
    Variable *evConnectionTimeOutInt = nullptr;
    uint16_t trackTxStartPoint = -1;
    uint16_t trackTxStopPoint = -1;
    MemVector<TxStartStopPoint> txStartPointParsed;
    MemVector<TxStartStopPoint> txStopPointParsed;
    bool isTxStartPoint(TxStartStopPoint check);
    bool isTxStopPoint(TxStartStopPoint check);

    bool parseTxStartStopPoint(const char *src, MemVector<TxStartStopPoint>& dst);

public:
    TransactionService(Context& context);

    void loop();

    Evse *getEvse(unsigned int evseId);

    RequestStartStopStatus requestStartTransaction(unsigned int evseId, unsigned int remoteStartId, IdToken idToken, char *transactionIdOut); //ChargingProfile, GroupIdToken not supported yet

    RequestStartStopStatus requestStopTransaction(const char *transactionId);
};

} // namespace MicroOcpp

#endif // MO_ENABLE_V201

#endif
