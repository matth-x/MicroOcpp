// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef TRANSACTION_H
#define TRANSACTION_H

#ifdef __cplusplus

#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Operations/CiStrings.h>

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
public:
    void setRequested() {this->requested = true;}
    bool isRequested() {return requested;}
    void confirm() {confirmed = true;}
    bool isConfirmed() {return confirmed;}
};

class Transaction {
private:
    ConnectorTransactionStore& context;

    bool active = true; //once active is false, the tx must stop (or cannot start at all)

    /*
     * Attributes existing before StartTransaction
     */
    char idTag [IDTAG_LEN_MAX + 1] = {'\0'};
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

#include <MicroOcpp/Model/Authorization/IdToken.h>
#include <MicroOcpp/Model/ConnectorBase/EvseId.h>

#define MO_TXID_LEN_MAX 36

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

class Transaction {
public:

    // ReasonEnumType (3.67)
    enum class StopReason : uint8_t {
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
    bool trackEvConnected;
    bool trackAuthorized;
    bool trackDataSigned;
    bool trackPowerPathClosed;
    bool trackEnergyTransfer;

    /*
     * Transaction lifecycle
     */
    bool active = true; //once active is false, the tx must stop (or cannot start at all)
    bool started = false; //if a TxEvent with event type TxStarted has been initiated
    bool stopped = false; //if a TxEvent with event type TxEnded has been initiated
    bool stoppedConfirmed = false; //if all TxEvents have been sent to the server (acknowledged or aborted)

    /*
     * Global transaction data
     */
    bool isAuthorized = false;    //if the given idToken was authorized
    bool isDeauthorized = false;  //if the server revoked a local authorization
    unsigned int seqNoCounter = 0; // increment by 1 for each event
    IdToken idToken;
    Timestamp beginTimestamp = MIN_TIME;
    char transactionId [MO_TXID_LEN_MAX + 1] = {'\0'};
    int remoteStartId = -1;

    bool idTokenTransmitted = true;

    bool evConnectionTimeoutListen = true;

    StopReason stopReason = StopReason::UNDEFINED;
    TransactionEventTriggerReason stopTrigger = TransactionEventTriggerReason::UNDEFINED;
    std::unique_ptr<IdToken> stopIdToken; // if null, then stopIdToken equals idToken
    bool stopIdTokenTransmitted = true;
};

// TransactionEventRequest (1.60.1)
class TransactionEventData {
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
    std::shared_ptr<Transaction> transaction;
    Type eventType;
    Timestamp timestamp;
    TransactionEventTriggerReason triggerReason;
    const unsigned int seqNo;
    bool offline = false;
    int numberOfPhasesUsed = -1;
    int cableMaxCurrent = -1;
    int reservationId = -1;

    // TransactionType (2.48)
    ChargingState chargingState = ChargingState::UNDEFINED;
    //int timeSpentCharging = 0; // not supported
    std::unique_ptr<IdToken> idToken;
    EvseId evse = -1;
    //meterValue not supported

    TransactionEventData(std::shared_ptr<Transaction> transaction, unsigned int seqNo) : transaction(transaction), seqNo(seqNo) { }
};

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

bool ocpp_tx_getStopTimestamp(OCPP_Transaction *tx, char *buf, size_t len);

const char *ocpp_tx_getStopReason(OCPP_Transaction *tx);

#ifdef __cplusplus
} //end extern "C"
#endif

#endif
