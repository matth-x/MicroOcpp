// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_MICROOCPP_H
#define MO_MICROOCPP_H

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/FtpMbedTLS.h>
#include <MicroOcpp/Model/Availability/AvailabilityDefs.h>
#include <MicroOcpp/Model/Boot/BootNotificationData.h>
#include <MicroOcpp/Model/Common/Mutability.h>
#include <MicroOcpp/Model/Metering/MeterValue.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlDefs.h>
#include <MicroOcpp/Model/Reset/ResetDefs.h>
#include <MicroOcpp/Model/SmartCharging/SmartChargingModel.h>
#include <MicroOcpp/Model/Transactions/TransactionDefs.h>
#include <MicroOcpp/Version.h>

/*
 * Basic integration
 */

//Begin the lifecycle of MO. Now, it is possible to set up the library. After configuring MO,
//call mo_setup() to finalize the setup
bool mo_initialize();

//End the lifecycle of MO and free all resources
void mo_deinitialize();

//Returns if library is initialized
bool mo_isInitialized();

//Returns Context handle to pass around API functions
MO_Context *mo_getApiContext();

#if MO_WS_USE == MO_WS_ARDUINO
/*
 * Setup MO with links2004/WebSockets library. Only available on Arduino, for other platforms set custom
 * WebSockets adapter (see examples folder). `backendUrl`, `chargeBoxId`, `authorizationKey` and
 * `CA_cert` are zero-copy and must remain valid until `mo_setup()`. `CA_cert` is zero-copy and must
 * outlive the MO lifecycle.
 * 
 * If the connections fails, please refer to 
 * https://github.com/matth-x/MicroOcpp/issues/36#issuecomment-989716573 for recommendations on
 * how to track down the issue with the connection.
 */
bool mo_setWebsocketUrl(
        const char *backendUrl,       //e.g. "wss://example.com:8443/steve/websocket/CentralSystemService". Must be defined
        const char *chargeBoxId,      //e.g. "charger001". Can be NULL
        const char *authorizationKey, //authorizationKey present in the websocket message header. Can be NULL. Set this to enable OCPP Security Profile 2
        const char *CA_cert);         //TLS certificate. Can be NULL. Set this to enable OCPP Security Profile 2
#endif

#if __cplusplus
/* Set a WebSocket Client. Required if ArduinoWebsockets is not supported. This library requires
 * that you handle establishing the connection and keeping it alive. MO does not take ownership
 * of the passed `connection` object, i.e. it must be destroyed after `mo_deinitialize()` Please
 * refer to https://github.com/matth-x/MicroOcpp/tree/main/examples/ESP-TLS for an example how to
 * use it.
 * 
 * This GitHub project also delivers an Connection implementation based on links2004/WebSockets. If
 * you need another WebSockets implementation, you can subclass the Connection class and pass it to
 * this initialize() function. Please refer to
 * https://github.com/OpenEVSE/ESP32_WiFi_V4.x/blob/master/src/MongooseConnectionClient.cpp for
 * an example.
 */
void mo_setConnection(MicroOcpp::Connection *connection);
void mo_setConnection2(MO_Context *ctx, MicroOcpp::Connection *connection);
#endif

#if MO_USE_FILEAPI != MO_CUSTOM_FS
//Set if MO can use the filesystem and if it needs to mount it. See "FilesystemAdapter.h" for all options
void mo_setDefaultFilesystemConfig(MO_FilesystemOpt opt);
void mo_setDefaultFilesystemConfig2(MO_Context *ctx, MO_FilesystemOpt opt, const char *pathPrefix);
#endif //#if MO_USE_FILEAPI != MO_CUSTOM_FS

//Set the OCPP version `MO_OCPP_V16` or `MO_OCPP_V201`. Works only if build flags `MO_ENABLE_V16` or
//`MO_ENABLE_V201` are set to 1
void mo_setOcppVersion(int ocppVersion);
void mo_setOcppVersion2(MO_Context *ctx, int ocppVersion);

//Set BootNotification fields (for more options, use `mo_setBootNotificationData2()`). For a description of
//the fields, refer to OCPP 1.6 or 2.0.1 specification
bool mo_setBootNotificationData(
        const char *chargePointModel,   //model name of this charger, e.g. "Demo Charger"
        const char *chargePointVendor); //brand name, e.g. "My Company Ltd."

bool mo_setBootNotificationData2(MO_Context *ctx, MO_BootNotificationData bnData); //see "BootNotificationData.h" for example usage


/*
 * Define the Inputs and Outputs of this library.
 * 
 * This library interacts with the hardware of your charger by Inputs and Outputs. Inputs and Outputs
 * are callbacks which read information from the EVSE or control the behavior of the EVSE.
 * 
 * An Input is a function which returns the current state of a variable of the EVSE. For example, if
 * the energy meter stores the energy register in the global variable `e_reg`, then you can allow
 * this library to read it by defining the following Input and passing it to the library.
 * ```
 *     setEnergyMeterInput([] () {
 *         return e_reg;
 *     });
 * ```
 * 
 * An Output is a callback which gets state value from the OCPP library and applies it to the EVSE.
 * For example, to let Smart Charging control the PWM signal of the Control Pilot, define the
 * following Output and pass it to the library.
 * ```
 *     setSmartChargingPowerOutput([] (float p_max) {
 *         pwm = p_max / PWM_FACTOR; //(simplified example)
 *     });
 * ```
 * 
 * Configure the library with Inputs and Outputs after mo_initialize() and before mo_setup().
 */

void mo_setConnectorPluggedInput(bool (*connectorPlugged)()); //Input about if an EV is plugged to this EVSE
void mo_setConnectorPluggedInput2(MO_Context *ctx, unsigned int evseId, bool (*connectorPlugged2)(unsigned int, void*), void *userData); //alternative with more args

bool mo_setEnergyMeterInput(int32_t (*energyInput)()); //Input of the electricity meter register in Wh
bool mo_setEnergyMeterInput2(MO_Context *ctx, unsigned int evseId, int32_t (*energyInput2)(MO_ReadingContext, unsigned int, void*), void *userData); //alternative with more args

bool mo_setPowerMeterInput(float (*powerInput)()); //Input of the power meter reading in W
bool mo_setPowerMeterInput2(MO_Context *ctx, unsigned int evseId, float (*powerInput2)(MO_ReadingContext, unsigned int, void*), void *userData); //alternative with more args

#if MO_ENABLE_SMARTCHARGING
//Smart Charging Output, alternative for Watts only, Current only, or Watts x Current x numberPhases (x phaseToUse)
//Only one of the Smart Charging Outputs can be set at a time.
//MO will execute the callback whenever the OCPP charging limit changes and will pass the limit for now
//to the callback. If OCPP does not define a limit, then MO passes the value -1 for "undefined".
bool mo_setSmartChargingPowerOutput(void (*powerLimitOutput)(float)); //Output (in Watts) for the Smart Charging limit
bool mo_setSmartChargingCurrentOutput(void (*currentLimitOutput)(float)); //Output (in Amps) for the Smart Charging limit
bool mo_setSmartChargingOutput(
        MO_Context *ctx,
        unsigned int evseId,
        void (*chargingLimitOutput)(MO_ChargeRate, unsigned int, void*),
        bool powerSupported, //charge limit defined as power supported
        bool currentSupported, //charge limit defined as current supported
        bool phases3to1Supported, //charger supports switching from 3 to 1 phase during charge
        bool phaseSwitchingSupported, //charger supports selecting the phase by server command. Ignored if OCPP 1.6
        void *userData); //Output (in Watts, Amps, numberPhases, phaseToUse) for the Smart Charging limit
#endif //MO_ENABLE_SMARTCHARGING

//See section with advanced Inputs and Outputs further below

//Finalize setup. After this, the library is ready to be used and mo_loop() can be run
bool mo_setup();

//Run library routines. To be called in the main loop (e.g. place it inside `loop()`)
void mo_loop();


/*
 * Transaction management.
 * 
 * OCPP 1.6 (2.0.1 see below):
 * Begin the transaction process and prepare it. When all conditions for the transaction are true,
 * eventually send a StartTransaction request to the OCPP server.
 * Conditions:
 *     1) the connector is operative (no faults reported, not set "Unavailable" by the backend)
 *     2) no reservation blocks the connector
 *     3) the idTag is authorized for charging. The transaction process will send an Authorize message
 *        to the server for approval, except if the charger is offline, then the Local Authorization
 *        rules will apply as in the specification.
 *     4) the vehicle is already plugged or will be plugged soon (only applicable if the
 *        ConnectorPlugged Input is set)
 * 
 * See beginTransaction_authorized for skipping steps 1) to 3)
 * 
 * Returns true if it was possible to create the transaction process. Returns
 * false if either another transaction process is still active or you need to try it again later.
 * 
 * OCPP 2.0.1:
 * Authorize a transaction. Like the OCPP 1.6 behavior, this should be called when the user swipes the
 * card to start charging, but the semantic is slightly different. This function begins the authorized
 * phase, but a transaction may already have started due to an earlier transaction start point.
 */
bool mo_beginTransaction(const char *idTag);
bool mo_beginTransaction2(MO_Context *ctx, unsigned int evseId, const char *idTag);

//Same as `mo_beginTransaction()`, but skip authorization. `parentIdTag` can be NULL
bool mo_beginTransaction_authorized(const char *idTag, const char *parentIdTag);
bool mo_beginTransaction_authorized2(MO_Context *ctx, unsigned int evseId, const char *idTag, const char *parentIdTag); //alternative with more args

#if MO_ENABLE_V201
//Same as mo_beginTransaction, but function name fits better for OCPP 2.0.1 and more options for 2.0.1.
//Attempt to authorize a pending transaction, or create new transactino to authorize. If local
//authorization is enabled, will search the local whitelist if server response takes too long.
//Backwards-compatible: if MO is initialized with 1.6, then this is the same as `mo_beginTransaction()`
bool mo_authorizeTransaction(const char *idToken); //idTokenType ISO14443 is assumed
bool mo_authorizeTransaction2(MO_Context *ctx, unsigned int evseId, const char *idToken, MO_IdTokenType type);
bool mo_authorizeTransaction3(MO_Context *ctx, unsigned int evseId, const char *idToken, MO_IdTokenType type, bool validateIdToken, const char *groupIdToken);

//Same as `mo_beginAuthorization()`, but skip authorization. `groupIdToken` can be NULL
bool mo_setTransactionAuthorized(const char *idToken, const char *groupIdToken); //idTokenType ISO14443 is assumed
bool mo_setTransactionAuthorized2(MO_Context *ctx, unsigned int evseId, const char *idToken, MO_IdTokenType type, const char *groupIdToken);
#endif //MO_ENABLE_V201

/*
 * OCPP 1.6 (2.0.1 see below):
 * End the transaction process if idTag is authorized to stop the transaction. The OCPP lib sends
 * a StopTransaction request if the following conditions are true:
 * Conditions:
 *     1) Currently, a transaction is running which hasn't been terminated yet AND
 *     2) idTag is either
 *         - nullptr OR
 *         - matches the idTag of beginTransaction (or RemoteStartTransaction) OR
 *         - [Planned, not released yet] is part of the current LocalList and the parentIdTag
 *           matches with the parentIdTag of beginTransaction.
 *         - [Planned, not released yet] If none of step 2) applies, then the OCPP lib will check
 *           the authorization status via an Authorize request
 * 
 * See endTransaction_authorized for skipping the authorization check, i.e. step 2)
 * 
 * If the transaction is ended by swiping an RFID card, then idTag should contain its identifier. If
 * charging stops for a different reason than swiping the card, idTag should be null or empty.
 * 
 * Please refer to OCPP 1.6 Specification - Edition 2 p. 90 for a list of valid reasons. `reason`
 * can also be nullptr.
 * 
 * It is safe to call this function at any time, i.e. when no transaction runs or when the transaction
 * has already been ended. For example you can place
 *     `endTransaction(nullptr, "Reboot");`
 * in the beginning of the program just to ensure that there is no transaction from a previous run.
 * 
 * If called with idTag=nullptr, this is functionally equivalent to
 *     `endTransaction_authorized(nullptr, reason);`
 * 
 * Returns true if there is a transaction which could eventually be ended by this action
 * 
 * OCPP 2.0.1:
 * End the user authorization. Like when running with OCPP 1.6, this should be called when the user
 * swipes the card to stop charging. The difference between the 1.6/2.0.1 behavior is that in 1.6,
 * endTransaction always sets the transaction inactive so that it wants to stop. In 2.0.1, this only
 * revokes the user authorization which may terminate the transaction but doesn't have to if the
 * transaction stop point is set to EvConnected.
 * 
 * Note: the stop reason parameter is ignored when running with OCPP 2.0.1. It's always Local
 */
bool mo_endTransaction(const char *idTag, const char *reason); //idTag and reason can be NULL
bool mo_endTransaction2(MO_Context *ctx, unsigned int evseId, const char *idTag, const char *reason);

//Same as `endTransaction()`, but skip authorization
bool mo_endTransaction_authorized(const char *idTag, const char *reason); //idTag and reason can be NULL
bool mo_endTransaction_authorized2(MO_Context *ctx, unsigned int evseId, const char *idTag, const char *reason);

#if MO_ENABLE_V201
//Same as mo_endTransaction, but function name fits better for OCPP 2.0.1 and more options for 2.0.1.
//Backwards-compatible: if MO is initialized with 1.6, then this is the same as `mo_endTransaction()`
bool mo_deauthorizeTransaction(const char *idToken); //idTokenType ISO14443 is assumed
bool mo_deauthorizeTransaction2(MO_Context *ctx, unsigned int evseId, const char *idToken, MO_IdTokenType type);
bool mo_deauthorizeTransaction3(MO_Context *ctx, unsigned int evseId, const char *idToken, MO_IdTokenType type, bool validateIdToken, const char *groupIdToken);

//Force transaction stop and set stoppedReason and stopTrigger correspondingly
//Backwards-compatible: if MO is initialized with 1.6, then this is the same as `mo_endTransaction()`
//and attempts to map 2.0.1 `stoppedReason` to 1.6 `reason`
bool mo_abortTransaction(MO_TxStoppedReason stoppedReason, MO_TxEventTriggerReason stopTrigger);
bool mo_abortTransaction2(MO_Context *ctx, unsigned int evseId, MO_TxStoppedReason stoppedReason, MO_TxEventTriggerReason stopTrigger);
#endif //MO_ENABLE_V201

/* 
 * Returns if according to OCPP, the EVSE is allowed to close provide charge now.
 *
 * If you integrate it into a J1772 charger, true means that the Control Pilot can send the PWM signal
 * and false means that the Control Pilot must be at a DC voltage.
 */
bool mo_ocppPermitsCharge();
bool mo_ocppPermitsCharge2(MO_Context *ctx, unsigned int evseId);

/*
 * Get information about the current Transaction lifecycle. A transaction can enter the following
 * states:
 *     - Idle: no transaction running or being started
 *     - Preparing: before a potential transaction
 *     - Aborted: transaction not started and never will be started
 *     - Running: transaction started and running
 *     - Running/StopTxAwait: transaction still running but will end at the next possible time
 *     - Finished: transaction stopped
 * 
 * isTransactionActive() and isTransactionRunning() give the status by combining them:
 * 
 *     State               | isTransactionActive() | isTransactionRunning()
 *     --------------------+-----------------------+-----------------------
 *     Preparing           | true                  | false
 *     Running             | true                  | true
 *     Running/StopTxAwait | false                 | true
 *     Finished / Aborted  |                       |
 *                  / Idle | false                 | false
 */
bool mo_isTransactionActive();
bool mo_isTransactionActive2(MO_Context *ctx, unsigned int evseId);
bool mo_isTransactionRunning();
bool mo_isTransactionRunning2(MO_Context *ctx, unsigned int evseId);

/*
 * Get the idTag which has been used to start the transaction. If no transaction process is
 * running, this function returns nullptr
 */
const char *mo_getTransactionIdTag();
const char *mo_getTransactionIdTag2(MO_Context *ctx, unsigned int evseId);

#if MO_ENABLE_V16
//Get the transactionId once the StartTransaction.conf from the server has arrived. Otherwise,
//returns -1
int mo_v16_getTransactionId();
int mo_v16_getTransactionId2(MO_Context *ctx, unsigned int evseId);
#endif //MO_ENABLE_V16

#if MO_ENABLE_V201
//Get the transactionId of the currently pending transaction. Returns NULL if no transaction is
//running. The returned string must be copied into own buffer, as it may be invalidated after call.
const char *mo_v201_getTransactionId();
const char *mo_v201_getTransactionId2(MO_Context *ctx, unsigned int evseId);
#endif //MO_ENABLE_V201

/*
 * Returns the latest MO_ChargePointStatus as reported via StatusNotification (standard OCPP data type)
 */
MO_ChargePointStatus mo_getChargePointStatus();
MO_ChargePointStatus mo_getChargePointStatus2(MO_Context *ctx, unsigned int evseId);


/*
 * Define the Inputs and Outputs of this library. (Advanced)
 * 
 * These Inputs and Outputs are optional depending on the use case of your charger. Set before `mo_setup()`
 */

void mo_setEvReadyInput(bool (*evReadyInput)()); //Input if EV is ready to charge (= J1772 State C)
void mo_setEvReadyInput2(MO_Context *ctx, unsigned int evseId, bool (*evReadyInput2)(unsigned int, void*), void *userData); //alternative with more args

void mo_setEvseReadyInput(bool (*evseReadyInput)()); //Input if EVSE allows charge (= PWM signal on)
void mo_setEvseReadyInput2(MO_Context *ctx, unsigned int evseId, bool (*evseReadyInput2)(unsigned int, void*), void *userData); //alternative with more args

bool mo_addErrorCodeInput(const char* (*errorCodeInput)()); //Input for Error codes (please refer to OCPP 1.6, Edit2, p. 71 and 72 for valid error codes)

#if MO_ENABLE_V16
//Input for Error codes + additional error data. See "Availability.h" for more docs
bool mo_v16_addErrorDataInput(MO_Context *ctx, unsigned int evseId, MO_ErrorData (*errorData)(unsigned int, void*), void *userData);
#endif //MO_ENABLE_V16

#if MO_ENABLE_V201
//Input to set EVSE into Faulted state
bool mo_v201_addFaultedInput(MO_Context *ctx, unsigned int evseId, bool (*faulted)(unsigned int, void*), void *userData);
#endif //MO_ENABLE_V201

//Input for further integer MeterValue types
bool mo_addMeterValueInputInt(int32_t (*meterInput)(), const char *measurand, const char *unit, const char *location, const char *phase);
bool mo_addMeterValueInputInt2(MO_Context *ctx, unsigned int evseId, int32_t (*meterInput)(MO_ReadingContext, unsigned int, void*), const char *measurand, const char *unit, const char *location, const char *phase, void *userData);

//Input for further float MeterValue types
bool mo_addMeterValueInputFloat(float (*meterInput)(), const char *measurand, const char *unit, const char *location, const char *phase, void *userData);
bool mo_addMeterValueInputFloat2(MO_Context *ctx, unsigned int evseId, float (*meterInput)(MO_ReadingContext, unsigned int, void*), const char *measurand, const char *unit, const char *location, const char *phase, void *userData);

//Input for further string MeterValue types. `meterInput` shall write the value into `buf` (which is `size` large) and return the number
//of characters written (not including the terminating 0). See "MeterValue.h" for a full description
bool mo_addMeterValueInputString(int (*meterInput)(char *buf, size_t size), const char *measurand, const char *unit, const char *location, const char *phase, void *userData);
bool mo_addMeterValueInputString2(MO_Context *ctx, unsigned int evseId, int (*meterInput)(char *buf, size_t size, MO_ReadingContext, unsigned int, void*), const char *measurand, const char *unit, const char *location, const char *phase, void *userData);

//Input for further MeterValue types with more options. See "MeterValue.h" for how to use it
bool mo_addMeterValueInput(MO_Context *ctx, unsigned int evseId, MO_MeterInput meterValueInput);

//Input if instead of Available, send StatusNotification Preparing / Finishing (OCPP 1.6) or Occupied (OCPP 2.0.1)
void mo_setOccupiedInput(bool (*occupied)());
void mo_setOccupiedInput2(MO_Context *ctx, unsigned int evseId, bool (*occupied2)(unsigned int, void*), void *userData);

void mo_setStartTxReadyInput(bool (*startTxReady)()); //Input if the charger is ready for StartTransaction
void mo_setStartTxReadyInput2(MO_Context *ctx, unsigned int evseId, bool (*startTxReady2)(unsigned int, void*), void *userData);

void mo_setStopTxReadyInput(bool (*stopTxReady)()); //Input if charger is ready for StopTransaction
void mo_setStopTxReadyInput2(MO_Context *ctx, unsigned int evseId, bool (*stopTxReady2)(unsigned int, void*), void *userData);

//Called when transaction state changes (see "TransactionDefs.h" for possible events). Transaction can be null
void mo_setTxNotificationOutput(void (*txNotificationOutput)(MO_TxNotification));
void mo_setTxNotificationOutput2(MO_Context *ctx, unsigned int evseId, void (*txNotificationOutput2)(MO_TxNotification, unsigned int, void*), void *userData);

#if MO_ENABLE_CONNECTOR_LOCK
/*
 * Set an InputOutput (reads and sets information at the same time) for forcing to unlock the
 * connector. Called as part of the OCPP operation "UnlockConnector"
 * Return values:
 *     - MO_UnlockConnectorResult_Pending if action needs more time to complete (MO will call this cb again later or eventually time out)
 *     - MO_UnlockConnectorResult_Unlocked if successful
 *     - MO_UnlockConnectorResult_UnlockFailed if not successful (e.g. lock stuck)
 */
void mo_setOnUnlockConnector(MO_UnlockConnectorResult (*onUnlockConnector)());
void mo_setOnUnlockConnector2(MO_Context *ctx, unsigned int evseId, MO_UnlockConnectorResult (*onUnlockConnector2)(unsigned int, void*), void *userData);
#endif //MO_ENABLE_CONNECTOR_LOCK


/*
 * Access further information about the internal state of the library
 */

bool mo_isOperative(); //if the charge point is operative (see OCPP1.6 Edit2, p. 45) and ready for transactions
bool mo_isOperative2(MO_Context *ctx, unsigned int evseId);

bool mo_isConnected(); //Returns WebSocket connection state
bool mo_isConnected2(MO_Context *ctx);

bool mo_isAcceptedByCsms(); //Returns if BootNotification has succeeded
bool mo_isAcceptedByCsms2(MO_Context *ctx);

int32_t mo_getUnixTime(); //Returns unix time (seconds since 1970-01-01 if known, 0 if unknown)
int32_t mo_getUnixTime2(MO_Context *ctx);
bool mo_setUnixTime(int32_t unixTime); //If the charger has an RTC, pre-init MO with the RTC time. On first connection, MO will sync with the server
bool mo_setUnixTime2(MO_Context *ctx, int32_t unixTime);

int32_t mo_getUptime(); //Returns seconds since boot
int32_t mo_getUptime2(MO_Context *ctx);

int mo_getOcppVersion(); //Returns either `MO_OCPP_V16` or `MO_OCPP_V201`
int mo_getOcppVersion2(MO_Context *ctx);

/*
 * Configure the device management. Set before `mo_setup()`
 */

//Reset handler. `onResetExecute` should reboot the controller. Already implemented for
//the ESP32 on Arduino
//Simpler reset handler version which supports OCPP 1.6 and 2.0.1
void mo_setOnResetExecute(void (*onResetExecute)());

#if MO_ENABLE_V16
//MO calls onResetNotify(isHard) before Reset. If you return false, Reset will be aborted. Optional for Reset feature
void mo_v16_setOnResetNotify(bool (*onResetNotify)(bool));
void mo_v16_setOnResetNotify2(MO_Context *ctx, bool (*onResetNotify2)(bool, void*), void *userData);

//Reset handler. This function should reboot this controller immediately. Already defined for the ESP32 on Arduino
void mo_v16_setOnResetExecute(void (*onResetExecute)(bool));
void mo_v16_setOnResetExecute2(MO_Context *ctx, bool (*onResetExecute2)(bool, void*), void *userData);
#endif //MO_ENABLE_V16

#if MO_ENABLE_V201
//MO calls onResetNotify(isHard) before Reset. If you return false, Reset will be aborted. Optional for Reset feature
void mo_v201_setOnResetNotify(bool (*onResetNotify)(MO_ResetType));
void mo_v201_setOnResetNotify2(MO_Context *ctx, unsigned int evseId, bool (*onResetNotify2)(MO_ResetType, unsigned int, void*), void *userData);

//Reset handler. This function should reboot this controller immediately. Already defined for the ESP32 on Arduino
void mo_v201_setOnResetExecute(void (*onResetExecute)()); //identical to `mo_setOnResetExecute`
void mo_v201_setOnResetExecute2(MO_Context *ctx, unsigned int evseId, bool (*onResetExecute2)(unsigned int, void*), void *userData);
#endif //MO_ENABLE_V201

#if MO_ENABLE_MBEDTLS
//Set FTP security parameters (e.g. client cert). See "FtpMbedTLS.h" for all options
//To use a custom FTP client, subclass `FtpClient` (see "Ftp.h") and pass to C++ `Context` object
void mo_setFtpConfig(MO_Context *ctx, MO_FTPConfig ftpConfig);

#if MO_ENABLE_DIAGNOSTICS
//Provide MO with diagnostics data for the "GetDiagnostics" operation. Should contain human-readable text which
//is useful for troubleshooting, e.g. heap occupation, all sensor readings, any device state and config which
//isn't directly exposed via OCPP operation.
//For more documentation, see "DiagnosticsService.h"
void mo_setDiagnosticsReader(MO_Context *ctx,
        size_t (*readBytes)(char *buf, size_t size, void *user_data), //reads diags into `buf`, having `size` bytes
        void(*onClose)(void *user_data), //MO calls this when diags upload is terminated
        void *userData); //implementer-defined pointer

void mo_setDiagnosticsFtpServerCert(MO_Context *ctx, const char *cert); //zero-copy, i.e. cert must outlive MO
#endif //MO_ENABLE_DIAGNOSTICS

#if MO_ENABLE_V16 && MO_ENABLE_FIRMWAREMANAGEMENT
//Implement custom Firmware upgrade. MO downloads the firmware binary via FTP and passes it chunk by chunk to
//the `writeBytes` callback. After the FTP download, MO terminates the firmware upgrade by calling `onClose`.
//For more documentation, see "FirmwareService.h"
//void mo_setFirmwareDownloadWriter(
//        size_t (*writeBytes)(const unsigned char *buf, size_t size, void *userData), //writes `buf`, having `size` bytes to OTA partition
//        void (*onClose)(MO_FtpCloseReason, void *userData), //MO calls this when the FTP was closed
//        void *userData); //implementer-defined pointer
//
//void mo_setFirmwareFtpServerCert(const char *cert); //zero-copy, i.e. cert must outlive MO
#endif //MO_ENABLE_V16 && MO_ENABLE_FIRMWAREMANAGEMENT

#endif //MO_ENABLE_MBEDTLS

/*
 * Transaction management (Advanced)
 */

bool mo_isTransactionAuthorized();
bool mo_isTransactionAuthorized2(MO_Context *ctx, unsigned int evseId);

//Returns if server has rejected transaction in StartTx or TransactionEvent response
bool mo_isTransactionDeauthorized();
bool mo_isTransactionDeauthorized2(MO_Context *ctx, unsigned int evseId);

//Returns start unix time of transaction. Returns 0 if unix time is not known
int32_t mo_getTransactionStartUnixTime();
int32_t mo_getTransactionStartUnixTime2(MO_Context *ctx, unsigned int evseId);

#if MO_ENABLE_V16
//Overrides start unix time of transaction
void mo_setTransactionStartUnixTime(int32_t unixTime);
void mo_setTransactionStartUnixTime2(MO_Context *ctx, unsigned int evseId, int32_t unixTime);

//Returns stop unix time of transaction. Returns 0 if unix time is not known
int32_t mo_getTransactionStopUnixTime();
int32_t mo_getTransactionStopUnixTime2(MO_Context *ctx, unsigned int evseId);

//Overrides stop unix time of transaction
void mo_setTransactionStopUnixTime(int32_t unixTime);
void mo_setTransactionStopUnixTime2(MO_Context *ctx, unsigned int evseId, int32_t unixTime);

//Returns meterStart of transaction. Returns -1 if meterStart is undefined
int32_t mo_getTransactionMeterStart();
int32_t mo_getTransactionMeterStart2(MO_Context *ctx, unsigned int evseId);

//Overrides meterStart of transaction
void mo_setTransactionMeterStart(int32_t meterStart);
void mo_setTransactionMeterStart2(MO_Context *ctx, unsigned int evseId, int32_t meterStart);

//Returns meterStop of transaction. Returns -1 if meterStop is undefined
int32_t mo_getTransactionMeterStop();
int32_t mo_getTransactionMeterStop2(MO_Context *ctx, unsigned int evseId);

//Overrides meterStop of transaction
void mo_setTransactionMeterStop(int32_t meterStop);
void mo_setTransactionMeterStop2(MO_Context *ctx, unsigned int evseId, int32_t meterStop);
#endif //MO_ENABLE_V16


#if MO_ENABLE_V16
/*
 * Access OCPP Configurations (OCPP 1.6)
 */
//Add new Config. `key` is zero-copy, i.e. must outlive the MO lifecycle. Add before `mo_setup()`.
//At first boot, MO uses `factoryDefault`. At subsequent boots, it loads the values from flash
//during `mo_setup()`
bool mo_declareConfigurationInt(const char *key, int factoryDefault, MO_Mutability mutability, bool persistent, bool rebootRequired); 
bool mo_declareConfigurationBool(const char *key, bool factoryDefault, MO_Mutability mutability, bool persistent, bool rebootRequired);
bool mo_declareConfigurationString(const char *key, const char *factoryDefault, MO_Mutability mutability, bool persistent, bool rebootRequired);

//Set Config value. Set after `mo_setup()`
bool mo_setConfigurationInt(const char *key, int value);
bool mo_setConfigurationBool(const char *key, bool value);
bool mo_setConfigurationString(const char *key, const char *value);

//Get Config value. MO writes the value into `valueOut`. Call after `mo_setup()`
bool mo_getConfigurationInt(const char *key, int *valueOut);
bool mo_getConfigurationBool(const char *key, bool *valueOut);
bool mo_getConfigurationString(const char *key, const char **valueOut); //need to copy `valueOut` into own buffer. `valueOut` may be invalidated after call
#endif //MO_ENABLE_V16

/*
 * Access OCPP Configurations (OCPP 1.6) or Variables (OCPP 2.0.1)
 * Backwards-compatible: if MO is initialized with OCPP 1.6, then these functions access Configurations
 */
//Add new Variable or Config. If running OCPP 2.0.1, `key16` can be NULL. If running OCPP 1.6,
//`component201` and `name201` can be NULL. `component201`, `name201` and `key16` are zero-copy,
//i.e. must outlive the MO lifecycle. Add before `mo_setup()`. At first boot, MO uses `factoryDefault`.
//At subsequent boots, it loads the values from flash during `mo_setup()`
bool mo_declareVarConfigInt(MO_Context *ctx, const char *component201, const char *name201, const char *key16, int factoryDefault, MO_Mutability mutability, bool persistent, bool rebootRequired);
bool mo_declareVarConfigBool(MO_Context *ctx, const char *component201, const char *name201, const char *key16, bool factoryDefault, MO_Mutability mutability, bool persistent, bool rebootRequired);
bool mo_declareVarConfigString(MO_Context *ctx, const char *component201, const char *name201, const char *key16, const char *factoryDefault, MO_Mutability mutability, bool persistent, bool rebootRequired);

//Set Variable or Config value. Set after `mo_setup()`. If running OCPP 2.0.1, `key16` can be NULL
bool mo_setVarConfigInt(MO_Context *ctx, const char *component201, const char *name201, const char *key16, int value);
bool mo_setVarConfigBool(MO_Context *ctx, const char *component201, const char *name201, const char *key16, bool value);
bool mo_setVarConfigString(MO_Context *ctx, const char *component201, const char *name201, const char *key16, const char *value);

//Get Config value. MO writes the value into `valueOut`. Call after `mo_setup()`. If running
//OCPP 2.0.1, `key16` can be NULL
bool mo_getVarConfigInt(MO_Context *ctx, const char *component201, const char *name201, const char *key16, int *valueOut);
bool mo_getVarConfigBool(MO_Context *ctx, const char *component201, const char *name201, const char *key16, bool *valueOut);
bool mo_getVarConfigString(MO_Context *ctx, const char *component201, const char *name201, const char *key16, const char **valueOut); //need to copy `valueOut` into own buffer. `valueOut` may be invalidated after call

/*
 * Callback-based Config or Variable integration (shared API between OCPP 1.6 and 2.0.1). This
 * allows to implement custom key-value stores and bypass the built-in implementation of MO.
 * MO uses `getValue` to read the Config/Variable value and `setValue` to update the value after
 * receiving a ChangeConfiguration or SetVariables request.
 */
//bool mo_addCustomVarConfigInt(const char *component201, const char *name201, const char *key16, int (*getValue)(const char*, const char*, const char*, void*), void (*setValue)(int, const char*, const char*, const char*, void*), void *userData);
//bool mo_addCustomVarConfigBool(const char *component201, const char *name201, const char *key16, bool (*getValue)(const char*, const char*, const char*, void*), void (*setValue)(bool, const char*, const char*, const char*, void*), void *userData);
//bool mo_addCustomVarConfigString(const char *component201, const char *name201, const char *key16, const char* (*getValue)(const char*, const char*, const char*, void*), bool (*setValue)(const char*, const char*, const char*, const char*, void*), void *userData);

/*
 * Add features and customize the behavior of the OCPP client
 */

#if __cplusplus
namespace MicroOcpp {
class Context;
}

//Get access to internal functions and data structures. The returned Context object allows
//you to bypass the facade functions of this header and implement custom functionality.
//To use, add `#include <MicroOcpp/Context.h>`
MicroOcpp::Context *mo_getContext();
MicroOcpp::Context *mo_getContext2(MO_Context *ctx);
#endif //__cplusplus

/*
 * Explicit context lifecycle. Avoids the internal `g_context` variable. Use if:
 *     - Explicit lifecycle is preferred (may fit the architecture better)
 *     - To run multiple instances of MO in parallel
 */
MO_Context *mo_initialize2();
bool mo_setup2(MO_Context *ctx);
void mo_deinitialize2(MO_Context *ctx);
void mo_loop2(MO_Context *ctx);

//Send operation manually and bypass MO business logic
bool mo_sendRequest(MO_Context *ctx,
        const char *operationType, //zero-copy, i.e. must outlive MO
        const char *payloadJson, //copied, i.e. can be invalidated
        void (*onResponse)(const char *payloadJson, void *userData),
        void (*onAbort)(void *userData),
        void *userData);

//Set custom operation handler for incoming reqeusts and bypass MO business logic
bool mo_setRequestHandler(MO_Context *ctx, const char *operationType,
        void (*onRequest)(const char *operationType, const char *payloadJson, int *userStatus, void *userData),
        int (*writeResponse)(const char *operationType, char *buf, size_t size, int userStatus, void *userData),
        void *userData);

//Sniff incoming requests without control over the response
bool mo_setOnReceiveRequest(MO_Context *ctx, const char *operationType,
        void (*onRequest)(const char *operationType, const char *payloadJson, void *userData),
        void *userData);

//Sniff outgoing responses generated by MO
bool mo_setOnSendConf(MO_Context *ctx, const char *operationType,
        void (*onSendConf)(const char *operationType, const char *payloadJson, void *userData),
        void *userData);

#endif
