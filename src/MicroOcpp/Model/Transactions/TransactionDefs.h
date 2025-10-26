// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_TRANSACTIONDEFS_H
#define MO_TRANSACTIONDEFS_H

#include <MicroOcpp/Version.h>

#ifdef __cplusplus
extern "C" {
#endif

#if MO_ENABLE_V16 || MO_ENABLE_V201

//MO_TxNotification - event from MO to the main firmware to notify it about transaction state changes
typedef enum {
    MO_TxNotification_UNDEFINED,

    //Authorization events
    MO_TxNotification_Authorized, //success
    MO_TxNotification_AuthorizationRejected, //IdTag/token not authorized
    MO_TxNotification_AuthorizationTimeout, //authorization failed - offline
    MO_TxNotification_ReservationConflict, //connector/evse reserved for other IdTag

    MO_TxNotification_ConnectionTimeout, //user took to long to plug vehicle after the authorization
    MO_TxNotification_DeAuthorized, //server rejected StartTx/TxEvent
    MO_TxNotification_RemoteStart, //authorized via RemoteStartTx/RequestStartTx
    MO_TxNotification_RemoteStop, //stopped via RemoteStopTx/RequestStopTx

    //Tx lifecycle events
    MO_TxNotification_StartTx, //entered running state (StartTx/TxEvent was initiated)
    MO_TxNotification_StopTx, //left running state (StopTx/TxEvent was initiated)
}   MO_TxNotification;

#endif //MO_ENABLE_V16 || MO_ENABLE_V201

#if MO_ENABLE_V201

// TriggerReasonEnumType (3.82)
typedef enum {
    MO_TxEventTriggerReason_UNDEFINED, // not part of OCPP
    MO_TxEventTriggerReason_Authorized,
    MO_TxEventTriggerReason_CablePluggedIn,
    MO_TxEventTriggerReason_ChargingRateChanged,
    MO_TxEventTriggerReason_ChargingStateChanged,
    MO_TxEventTriggerReason_Deauthorized,
    MO_TxEventTriggerReason_EnergyLimitReached,
    MO_TxEventTriggerReason_EVCommunicationLost,
    MO_TxEventTriggerReason_EVConnectTimeout,
    MO_TxEventTriggerReason_MeterValueClock,
    MO_TxEventTriggerReason_MeterValuePeriodic,
    MO_TxEventTriggerReason_TimeLimitReached,
    MO_TxEventTriggerReason_Trigger,
    MO_TxEventTriggerReason_UnlockCommand,
    MO_TxEventTriggerReason_StopAuthorized,
    MO_TxEventTriggerReason_EVDeparted,
    MO_TxEventTriggerReason_EVDetected,
    MO_TxEventTriggerReason_RemoteStop,
    MO_TxEventTriggerReason_RemoteStart,
    MO_TxEventTriggerReason_AbnormalCondition,
    MO_TxEventTriggerReason_SignedDataReceived,
    MO_TxEventTriggerReason_ResetCommand
}   MO_TxEventTriggerReason;

// ReasonEnumType (3.67)
typedef enum {
    MO_TxStoppedReason_UNDEFINED, // not part of OCPP
    MO_TxStoppedReason_DeAuthorized,
    MO_TxStoppedReason_EmergencyStop,
    MO_TxStoppedReason_EnergyLimitReached,
    MO_TxStoppedReason_EVDisconnected,
    MO_TxStoppedReason_GroundFault,
    MO_TxStoppedReason_ImmediateReset,
    MO_TxStoppedReason_Local,
    MO_TxStoppedReason_LocalOutOfCredit,
    MO_TxStoppedReason_MasterPass,
    MO_TxStoppedReason_Other,
    MO_TxStoppedReason_OvercurrentFault,
    MO_TxStoppedReason_PowerLoss,
    MO_TxStoppedReason_PowerQuality,
    MO_TxStoppedReason_Reboot,
    MO_TxStoppedReason_Remote,
    MO_TxStoppedReason_SOCLimitReached,
    MO_TxStoppedReason_StoppedByEV,
    MO_TxStoppedReason_TimeLimitReached,
    MO_TxStoppedReason_Timeout
}   MO_TxStoppedReason;

#endif //MO_ENABLE_V201

#ifdef __cplusplus
} //extern "C"
#endif

#endif
