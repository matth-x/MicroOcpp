// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_TRANSACTION_H
#define MO_TRANSACTION_H

#include <memory>
#include <limits>

#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Core/UuidUtils.h>
#include <MicroOcpp/Model/Authorization/IdToken.h>
#include <MicroOcpp/Model/Common/EvseId.h>
#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Model/Transactions/TransactionDefs.h>
#include <MicroOcpp/Version.h>

#define MO_TXNR_MAX 10000U //upper limit of txNr (internal usage). Must be at least 2*MO_TXRECORD_SIZE+1
#define MO_TXNR_DIGITS 4   //digits needed to print MAX_INDEX-1 (=9999, i.e. 4 digits)

#define MO_REASON_LEN_MAX 15

#if MO_ENABLE_V16

namespace MicroOcpp {
namespace Ocpp16 {

/*
 * A transaction is initiated by the client (charging station) and processed by the server (central system).
 * The client side of a transaction is all data that is generated or collected at the charging station. The
 * server side is all transaction data that is assigned by the central system.
 * 
 * See OCPP 1.6 Specification - Edition 2, sections 3.6, 4.8, 4.10 and 5.11. 
 */

class TransactionStoreEvse;

class SendStatus {
private:
    bool requested = false;
    bool confirmed = false;
    
    unsigned int opNr = 0;
    unsigned int attemptNr = 0;
    Timestamp attemptTime;
public:
    void setRequested() {this->requested = true;}
    bool isRequested() {return requested;}
    void confirm() {confirmed = true;}
    bool isConfirmed() {return confirmed;}
    void setOpNr(unsigned int opNr) {this->opNr = opNr;}
    unsigned int getOpNr() {return opNr;}
    void advanceAttemptNr() {attemptNr++;}
    void setAttemptNr(unsigned int attemptNr) {this->attemptNr = attemptNr;}
    unsigned int getAttemptNr() {return attemptNr;}
    const Timestamp& getAttemptTime() {return attemptTime;}
    void setAttemptTime(const Timestamp& timestamp) {attemptTime = timestamp;}
};

class Transaction : public MemoryManaged {
private:
    bool active = true; //once active is false, the tx must stop (or cannot start at all)

    /*
     * Attributes existing before StartTransaction
     */
    char idTag [MO_IDTAG_LEN_MAX + 1] = {'\0'};
    char parentIdTag [MO_IDTAG_LEN_MAX + 1] = {'\0'};
    bool authorized = false;    //if the given idTag was authorized
    bool deauthorized = false;  //if the server revoked a local authorization
    Timestamp begin_timestamp;
    int reservationId = -1;
    int txProfileId = -1;

    /*
     * Attributes of StartTransaction
     */
    SendStatus start_sync;
    int32_t start_meter = -1;           //meterStart of StartTx
    Timestamp start_timestamp;      //timestamp of StartTx; can be set before actually initiating
    int transactionId = -1; //only valid if confirmed = true

    #if MO_ENABLE_V201
    char transactionIdCompat [12] = {'\0'}; //v201 compat: provide txId as string too. Buffer dimensioned to hold max int value
    #endif //MO_ENABLE_V201

    /*
     * Attributes of StopTransaction
     */
    SendStatus stop_sync;
    char stop_idTag [MO_IDTAG_LEN_MAX + 1] = {'\0'};
    int32_t stop_meter = -1;
    Timestamp stop_timestamp;
    char stop_reason [MO_REASON_LEN_MAX + 1] = {'\0'};

    /*
     * General attributes
     */
    unsigned int connectorId = 0;
    unsigned int txNr = 0; //client-side key of this tx object (!= transactionId)

    bool silent = false; //silent Tx: process tx locally, without reporting to the server

    Vector<MeterValue*> meterValues;

public:
    Transaction(unsigned int connectorId, unsigned int txNr, bool silent = false);

    ~Transaction();

    /*
     * data assigned by OCPP server
     */
    int getTransactionId() {return transactionId;}
    #if MO_ENABLE_V201
    const char *getTransactionIdCompat() {return transactionIdCompat;}
    #endif //MO_ENABLE_V201
    bool isAuthorized() {return authorized;} //Authorize has been accepted
    bool isIdTagDeauthorized() {return deauthorized;} //StartTransaction has been rejected

    /*
     * Transaction life cycle
     */
    bool isRunning() {return start_sync.isRequested() && !stop_sync.isRequested();} //tx is running
    bool isActive() {return active;} //tx continues to run or is preparing
    bool isAborted() {return !start_sync.isRequested() && !active;} //tx ended before startTx was sent
    bool isCompleted() {return stop_sync.isConfirmed();} //tx ended and startTx and stopTx have been confirmed by server

    /*
     * Getters and setters for (mostly) internal use
     */
    void setInactive() {active = false;}

    bool setIdTag(const char *idTag);
    const char *getIdTag() {return idTag;}

    bool setParentIdTag(const char *idTag);
    const char *getParentIdTag() {return parentIdTag;}

    void setAuthorized() {authorized = true;}
    void setIdTagDeauthorized() {deauthorized = true;}

    void setBeginTimestamp(Timestamp timestamp) {begin_timestamp = timestamp;}
    const Timestamp& getBeginTimestamp() {return begin_timestamp;}

    void setReservationId(int reservationId) {this->reservationId = reservationId;}
    int getReservationId() {return reservationId;}

    void setTxProfileId(int txProfileId) {this->txProfileId = txProfileId;}
    int getTxProfileId() {return txProfileId;}

    SendStatus& getStartSync() {return start_sync;}

    void setMeterStart(int32_t meter) {start_meter = meter;}
    bool isMeterStartDefined() {return start_meter >= 0;}
    int32_t getMeterStart() {return start_meter;}

    void setStartTimestamp(Timestamp timestamp) {start_timestamp = timestamp;}
    const Timestamp& getStartTimestamp() {return start_timestamp;}

    void setTransactionId(int transactionId);

    SendStatus& getStopSync() {return stop_sync;}

    bool setStopIdTag(const char *idTag);
    const char *getStopIdTag() {return stop_idTag;}

    void setMeterStop(int32_t meter) {stop_meter = meter;}
    bool isMeterStopDefined() {return stop_meter >= 0;}
    int32_t getMeterStop() {return stop_meter;}

    void setStopTimestamp(Timestamp timestamp) {stop_timestamp = timestamp;}
    const Timestamp& getStopTimestamp() {return stop_timestamp;}

    bool setStopReason(const char *reason);
    const char *getStopReason() {return stop_reason;}

    void setConnectorId(unsigned int connectorId) {this->connectorId = connectorId;}
    unsigned int getConnectorId() {return connectorId;}

    void setTxNr(unsigned int txNr) {this->txNr = txNr;}
    unsigned int getTxNr() {return txNr;} //internal primary key of this tx object

    void setSilent() {silent = true;}
    bool isSilent() {return silent;} //no data will be sent to server and server will not assign transactionId

    Vector<MeterValue*>& getTxMeterValues() {return meterValues;}
};

} //namespace Ocpp16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16

#if MO_ENABLE_V201

#define MO_TXID_SIZE MO_UUID_STR_SIZE

#ifndef MO_SAMPLEDDATATXENDED_SIZE_MAX
#define MO_SAMPLEDDATATXENDED_SIZE_MAX 5
#endif

namespace MicroOcpp {

class Clock;

namespace Ocpp201 {

class Transaction : public MemoryManaged {
private:
    Clock& clock;
public:
    /*
     * Transaction substates. Notify server about any change when transaction is running
     */
    //bool trackParkingBayOccupancy; // not supported
    bool trackEvConnected = false;
    bool trackAuthorized = false;
    bool trackDataSigned = false;
    bool trackPowerPathClosed = false;
    bool trackEnergyTransfer = false;

    /*
     * Transaction lifecycle
     */
    bool active = true; //once active is false, the tx must stop (or cannot start at all)
    bool started = false; //if a TxEvent with event type TxStarted has been initiated
    bool stopped = false; //if a TxEvent with event type TxEnded has been initiated

    /*
     * Global transaction data
     */
    bool isAuthorizationActive = false; //period between beginAuthorization and endAuthorization
    bool isAuthorized = false;    //if the given idToken was authorized
    bool isDeauthorized = false;  //if the server revoked a local authorization
    IdToken idToken;
    Timestamp beginTimestamp;
    Timestamp startTimestamp;
    char transactionId [MO_TXID_SIZE] = {'\0'};
    int remoteStartId = -1;
    int txProfileId = -1;

    //if to fill next TxEvent with optional fields
    bool notifyEvseId = false;
    bool notifyIdToken = false;
    bool notifyStopIdToken = false;
    bool notifyReservationId = false;
    bool notifyChargingState = false;
    bool notifyRemoteStartId = false;

    bool evConnectionTimeoutListen = true;

    MO_TxStoppedReason stoppedReason = MO_TxStoppedReason_UNDEFINED;
    MO_TxEventTriggerReason stopTrigger = MO_TxEventTriggerReason_UNDEFINED;
    std::unique_ptr<IdToken> stopIdToken; // if null, then stopIdToken equals idToken

    /*
     * Tx-related metering
     */

    Vector<std::unique_ptr<MeterValue>> sampledDataTxEnded;

    Timestamp lastSampleTimeTxUpdated;
    Timestamp lastSampleTimeTxEnded;

    /*
     * Attributes for internal store
     */
    unsigned int evseId = 0;
    unsigned int txNr = 0; //internal key attribute (!= transactionId); {evseId*txNr} is unique key

    unsigned int seqNoEnd = 0; // increment by 1 for each event
    Vector<unsigned int> seqNos; //track stored txEvents

    bool silent = false; //silent Tx: process tx locally, without reporting to the server

    Transaction(Clock& clock) :
            MemoryManaged("v201.Transactions.Transaction"),
            clock(clock),
            sampledDataTxEnded(makeVector<std::unique_ptr<MeterValue>>(getMemoryTag())),
            seqNos(makeVector<unsigned int>(getMemoryTag())) { }

    void addSampledDataTxEnded(std::unique_ptr<MeterValue> mv) {
        if (sampledDataTxEnded.size() >= MO_SAMPLEDDATATXENDED_SIZE_MAX) {
            int32_t deltaMin = std::numeric_limits<int32_t>::max();
            size_t indexMin = sampledDataTxEnded.size();
            for (size_t i = 1; i + 1 <= sampledDataTxEnded.size(); i++) {
                size_t t0 = sampledDataTxEnded.size() - i - 1;
                size_t t1 = sampledDataTxEnded.size() - i;

                int32_t dt;
                if (!clock.delta(sampledDataTxEnded[t1]->timestamp, sampledDataTxEnded[t0]->timestamp, dt)) {
                    dt = std::numeric_limits<int32_t>::max();
                }

                if (dt < deltaMin) {
                    deltaMin = dt;
                    indexMin = t1;
                }
            }

            sampledDataTxEnded.erase(sampledDataTxEnded.begin() + indexMin);
        }

        sampledDataTxEnded.push_back(std::move(mv));
    }
};

// TransactionEventRequest (1.60.1)
class TransactionEventData : public MemoryManaged {
public:

    // TransactionEventEnumType (3.80)
    enum class Type : uint8_t {
        Ended,
        Started,
        Updated
    };

    // ChargingStateEnumType (3.16)
    enum class ChargingState : uint8_t {
        UNDEFINED, // not part of OCPP
        Charging,
        EVConnected,
        SuspendedEV,
        SuspendedEVSE,
        Idle
    };

    Transaction *transaction;
    Type eventType;
    Timestamp timestamp;
    MO_TxEventTriggerReason triggerReason;
    const unsigned int seqNo;
    bool offline = false;
    int numberOfPhasesUsed = -1;
    int cableMaxCurrent = -1;
    int reservationId = -1;
    int remoteStartId = -1;

    // TransactionType (2.48)
    ChargingState chargingState = ChargingState::UNDEFINED;
    //int timeSpentCharging = 0; // not supported
    std::unique_ptr<IdToken> idToken;
    EvseId evse = -1;
    Vector<std::unique_ptr<MeterValue>> meterValue;
    
    unsigned int opNr = 0;
    unsigned int attemptNr = 0;
    Timestamp attemptTime;

    TransactionEventData(Transaction *transaction, unsigned int seqNo) : MemoryManaged("v201.Transactions.TransactionEventData"), transaction(transaction), seqNo(seqNo), meterValue(makeVector<std::unique_ptr<MeterValue>>(getMemoryTag())) { }
};

const char *serializeTransactionStoppedReason(MO_TxStoppedReason stoppedReason);
bool deserializeTransactionStoppedReason(const char *stoppedReasonCstr, MO_TxStoppedReason& stoppedReasonOut);

const char *serializeTransactionEventType(TransactionEventData::Type type);
bool deserializeTransactionEventType(const char *typeCstr, TransactionEventData::Type& typeOut);

const char *serializeTxEventTriggerReason(MO_TxEventTriggerReason triggerReason);
bool deserializeTxEventTriggerReason(const char *triggerReasonCstr, MO_TxEventTriggerReason& triggerReasonOut);

const char *serializeTransactionEventChargingState(TransactionEventData::ChargingState chargingState);
bool deserializeTransactionEventChargingState(const char *chargingStateCstr, TransactionEventData::ChargingState& chargingStateOut);


} //namespace Ocpp201
} //namespace MicroOcpp

#endif //MO_ENABLE_V201
#endif
