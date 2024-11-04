// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef TRANSACTION_H
#define TRANSACTION_H

/* General Tx defs */
#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

//TxNotification - event from MO to the main firmware to notify it about transaction state changes
typedef enum {
    UNDEFINED,

    //Authorization events
    TxNotification_Authorized, //success
    TxNotification_AuthorizationRejected, //IdTag/token not authorized
    TxNotification_AuthorizationTimeout, //authorization failed - offline
    TxNotification_ReservationConflict, //connector/evse reserved for other IdTag

    TxNotification_ConnectionTimeout, //user took to long to plug vehicle after the authorization
    TxNotification_DeAuthorized, //server rejected StartTx/TxEvent
    TxNotification_RemoteStart, //authorized via RemoteStartTx/RequestStartTx
    TxNotification_RemoteStop, //stopped via RemoteStopTx/RequestStopTx

    //Tx lifecycle events
    TxNotification_StartTx, //entered running state (StartTx/TxEvent was initiated)
    TxNotification_StopTx, //left running state (StopTx/TxEvent was initiated)
}   TxNotification;

#ifdef __cplusplus
}
#endif //__cplusplus

#ifdef __cplusplus

#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Operations/CiStrings.h>

#define MAX_TX_CNT 100000U //upper limit of txNr (internal usage). Must be at least 2*MO_TXRECORD_SIZE+1

namespace MicroOcpp {

/*
 * A transaction is initiated by the client (charging station) and processed by the server (central system).
 * The client side of a transaction is all data that is generated or collected at the charging station. The
 * server side is all transaction data that is assigned by the central system.
 * 
 * See OCPP 1.6 Specification - Edition 2, sections 3.6, 4.8, 4.10 and 5.11. 
 */

class ConnectorTransactionStore;

class SendStatus {
private:
    bool requested = false;
    bool confirmed = false;
    
    unsigned int opNr = 0;
    unsigned int attemptNr = 0;
    Timestamp attemptTime = MIN_TIME;
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
    ConnectorTransactionStore& context;

    bool active = true; //once active is false, the tx must stop (or cannot start at all)

    /*
     * Attributes existing before StartTransaction
     */
    char idTag [IDTAG_LEN_MAX + 1] = {'\0'};
    char parentIdTag [IDTAG_LEN_MAX + 1] = {'\0'};
    bool authorized = false;    //if the given idTag was authorized
    bool deauthorized = false;  //if the server revoked a local authorization
    Timestamp begin_timestamp = MIN_TIME;
    int reservationId = -1;
    int txProfileId = -1;

    /*
     * Attributes of StartTransaction
     */
    SendStatus start_sync;
    int32_t start_meter = -1;           //meterStart of StartTx
    Timestamp start_timestamp = MIN_TIME;      //timestamp of StartTx; can be set before actually initiating
    uint16_t start_bootNr = 0;
    int transactionId = -1; //only valid if confirmed = true

    /*
     * Attributes of StopTransaction
     */
    SendStatus stop_sync;
    char stop_idTag [IDTAG_LEN_MAX + 1] = {'\0'};
    int32_t stop_meter = -1;
    Timestamp stop_timestamp = MIN_TIME;
    uint16_t stop_bootNr = 0;
    char stop_reason [REASON_LEN_MAX + 1] = {'\0'};

    /*
     * General attributes
     */
    unsigned int connectorId = 0;
    unsigned int txNr = 0; //client-side key of this tx object (!= transactionId)

    bool silent = false; //silent Tx: process tx locally, without reporting to the server

public:
    Transaction(ConnectorTransactionStore& context, unsigned int connectorId, unsigned int txNr, bool silent = false) : 
                MemoryManaged("v16.Transactions.Transaction"),
                context(context),
                connectorId(connectorId), 
                txNr(txNr),
                silent(silent) {}

    /*
     * data assigned by OCPP server
     */
    int getTransactionId() {return transactionId;}
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
     * After modifying a field of tx, commit to make the data persistent
     */
    bool commit();

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

    void setStartBootNr(uint16_t bootNr) {start_bootNr = bootNr;} 
    uint16_t getStartBootNr() {return start_bootNr;}

    void setTransactionId(int transactionId) {this->transactionId = transactionId;}

    SendStatus& getStopSync() {return stop_sync;}

    bool setStopIdTag(const char *idTag);
    const char *getStopIdTag() {return stop_idTag;}

    void setMeterStop(int32_t meter) {stop_meter = meter;}
    bool isMeterStopDefined() {return stop_meter >= 0;}
    int32_t getMeterStop() {return stop_meter;}

    void setStopTimestamp(Timestamp timestamp) {stop_timestamp = timestamp;}
    const Timestamp& getStopTimestamp() {return stop_timestamp;}

    void setStopBootNr(uint16_t bootNr) {stop_bootNr = bootNr;} 
    uint16_t getStopBootNr() {return stop_bootNr;}

    bool setStopReason(const char *reason);
    const char *getStopReason() {return stop_reason;}

    void setConnectorId(unsigned int connectorId) {this->connectorId = connectorId;}
    unsigned int getConnectorId() {return connectorId;}

    void setTxNr(unsigned int txNr) {this->txNr = txNr;}
    unsigned int getTxNr() {return txNr;} //internal primary key of this tx object

    void setSilent() {silent = true;}
    bool isSilent() {return silent;} //no data will be sent to server and server will not assign transactionId
};

} // namespace MicroOcpp

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <memory>
#include <limits>

#include <MicroOcpp/Model/Transactions/TransactionDefs.h>
#include <MicroOcpp/Model/Authorization/IdToken.h>
#include <MicroOcpp/Model/ConnectorBase/EvseId.h>
#include <MicroOcpp/Model/Metering/MeterValuesV201.h>

#ifndef MO_SAMPLEDDATATXENDED_SIZE_MAX
#define MO_SAMPLEDDATATXENDED_SIZE_MAX 5
#endif

namespace MicroOcpp {
namespace Ocpp201 {

// TriggerReasonEnumType (3.82)
enum class TransactionEventTriggerReason : uint8_t {
    UNDEFINED, // not part of OCPP
    Authorized,
    CablePluggedIn,
    ChargingRateChanged,
    ChargingStateChanged,
    Deauthorized,
    EnergyLimitReached,
    EVCommunicationLost,
    EVConnectTimeout,
    MeterValueClock,
    MeterValuePeriodic,
    TimeLimitReached,
    Trigger,
    UnlockCommand,
    StopAuthorized,
    EVDeparted,
    EVDetected,
    RemoteStop,
    RemoteStart,
    AbnormalCondition,
    SignedDataReceived,
    ResetCommand
};

class Transaction : public MemoryManaged {
public:

    // ReasonEnumType (3.67)
    enum class StoppedReason : uint8_t {
        UNDEFINED, // not part of OCPP
        DeAuthorized,
        EmergencyStop,
        EnergyLimitReached,
        EVDisconnected,
        GroundFault,
        ImmediateReset,
        Local,
        LocalOutOfCredit,
        MasterPass,
        Other,
        OvercurrentFault,
        PowerLoss,
        PowerQuality,
        Reboot,
        Remote,
        SOCLimitReached,
        StoppedByEV,
        TimeLimitReached,
        Timeout
    };

//private:
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
    Timestamp beginTimestamp = MIN_TIME;
    char transactionId [MO_TXID_LEN_MAX + 1] = {'\0'};
    int remoteStartId = -1;

    //if to fill next TxEvent with optional fields
    bool notifyEvseId = false;
    bool notifyIdToken = false;
    bool notifyStopIdToken = false;
    bool notifyReservationId = false;
    bool notifyChargingState = false;
    bool notifyRemoteStartId = false;

    bool evConnectionTimeoutListen = true;

    StoppedReason stoppedReason = StoppedReason::UNDEFINED;
    TransactionEventTriggerReason stopTrigger = TransactionEventTriggerReason::UNDEFINED;
    std::unique_ptr<IdToken> stopIdToken; // if null, then stopIdToken equals idToken

    /*
     * Tx-related metering
     */

    Vector<std::unique_ptr<Ocpp201::MeterValue>> sampledDataTxEnded;

    unsigned long lastSampleTimeTxUpdated = 0; //0 means not charging right now
    unsigned long lastSampleTimeTxEnded = 0;

    /*
     * Attributes for internal store
     */
    unsigned int evseId = 0;
    unsigned int txNr = 0; //internal key attribute (!= transactionId); {evseId*txNr} is unique key

    unsigned int seqNoEnd = 0; // increment by 1 for each event
    Vector<unsigned int> seqNos; //track stored txEvents

    bool silent = false; //silent Tx: process tx locally, without reporting to the server

    Transaction() :
            MemoryManaged("v201.Transactions.Transaction"),
            sampledDataTxEnded(makeVector<std::unique_ptr<Ocpp201::MeterValue>>(getMemoryTag())),
            seqNos(makeVector<unsigned int>(getMemoryTag())) { }

    void addSampledDataTxEnded(std::unique_ptr<Ocpp201::MeterValue> mv) {
        if (sampledDataTxEnded.size() >= MO_SAMPLEDDATATXENDED_SIZE_MAX) {
            int deltaMin = std::numeric_limits<int>::max();
            size_t indexMin = sampledDataTxEnded.size();
            for (size_t i = 1; i + 1 <= sampledDataTxEnded.size(); i++) {
                size_t t0 = sampledDataTxEnded.size() - i - 1;
                size_t t1 = sampledDataTxEnded.size() - i;

                auto delta = sampledDataTxEnded[t1]->getTimestamp() - sampledDataTxEnded[t0]->getTimestamp();

                if (delta < deltaMin) {
                    deltaMin = delta;
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

//private:
    Transaction *transaction;
    Type eventType;
    Timestamp timestamp;
    uint16_t bootNr = 0;
    TransactionEventTriggerReason triggerReason;
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
    //meterValue not supported
    Vector<std::unique_ptr<MeterValue>> meterValue;
    
    unsigned int opNr = 0;
    unsigned int attemptNr = 0;
    Timestamp attemptTime = MIN_TIME;

    TransactionEventData(Transaction *transaction, unsigned int seqNo) : MemoryManaged("v201.Transactions.TransactionEventData"), transaction(transaction), seqNo(seqNo), meterValue(makeVector<std::unique_ptr<MeterValue>>(getMemoryTag())) { }
};

const char *serializeTransactionStoppedReason(Transaction::StoppedReason stoppedReason);
bool deserializeTransactionStoppedReason(const char *stoppedReasonCstr, Transaction::StoppedReason& stoppedReasonOut);

const char *serializeTransactionEventType(TransactionEventData::Type type);
bool deserializeTransactionEventType(const char *typeCstr, TransactionEventData::Type& typeOut);

const char *serializeTransactionEventTriggerReason(TransactionEventTriggerReason triggerReason);
bool deserializeTransactionEventTriggerReason(const char *triggerReasonCstr, TransactionEventTriggerReason& triggerReasonOut);

const char *serializeTransactionEventChargingState(TransactionEventData::ChargingState chargingState);
bool deserializeTransactionEventChargingState(const char *chargingStateCstr, TransactionEventData::ChargingState& chargingStateOut);


} // namespace Ocpp201
} // namespace MicroOcpp

#endif // MO_ENABLE_V201

extern "C" {
#endif //__cplusplus

struct OCPP_Transaction;
typedef struct OCPP_Transaction OCPP_Transaction;

int ocpp_tx_getTransactionId(OCPP_Transaction *tx);
bool ocpp_tx_isAuthorized(OCPP_Transaction *tx);
bool ocpp_tx_isIdTagDeauthorized(OCPP_Transaction *tx);

bool ocpp_tx_isRunning(OCPP_Transaction *tx);
bool ocpp_tx_isActive(OCPP_Transaction *tx);
bool ocpp_tx_isAborted(OCPP_Transaction *tx);
bool ocpp_tx_isCompleted(OCPP_Transaction *tx);

const char *ocpp_tx_getIdTag(OCPP_Transaction *tx);

bool ocpp_tx_getBeginTimestamp(OCPP_Transaction *tx, char *buf, size_t len);

int32_t ocpp_tx_getMeterStart(OCPP_Transaction *tx);

bool ocpp_tx_getStartTimestamp(OCPP_Transaction *tx, char *buf, size_t len);

const char *ocpp_tx_getStopIdTag(OCPP_Transaction *tx);

int32_t ocpp_tx_getMeterStop(OCPP_Transaction *tx);
void ocpp_tx_setMeterStop(OCPP_Transaction* tx, int32_t meter);

bool ocpp_tx_getStopTimestamp(OCPP_Transaction *tx, char *buf, size_t len);

const char *ocpp_tx_getStopReason(OCPP_Transaction *tx);

#ifdef __cplusplus
} //end extern "C"
#endif

#endif
