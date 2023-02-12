// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef ARDUINOOCPP_H
#define ARDUINOOCPP_H

#include <ArduinoJson.h>
#include <memory>
#include <functional>

#include <ArduinoOcpp/Core/ConfigurationOptions.h>
#include <ArduinoOcpp/Core/OcppTime.h>
#include <ArduinoOcpp/Core/OcppOperationCallbacks.h>
#include <ArduinoOcpp/Core/OcppOperationTimeout.h>
#include <ArduinoOcpp/Core/OcppSocket.h>
#include <ArduinoOcpp/Core/PollResult.h>
#include <ArduinoOcpp/Tasks/Transactions/TransactionPrerequisites.h>
#include <ArduinoOcpp/Tasks/Metering/SampledValue.h>

using ArduinoOcpp::OnReceiveConfListener;
using ArduinoOcpp::OnReceiveReqListener;
using ArduinoOcpp::OnSendConfListener;
using ArduinoOcpp::OnAbortListener;
using ArduinoOcpp::OnTimeoutListener;
using ArduinoOcpp::OnReceiveErrorListener;

using ArduinoOcpp::Timeout;

#ifndef AO_CUSTOM_WS
//use links2004/WebSockets library

/*
 * Initialize the library with the OCPP URL, EVSE voltage and filesystem configuration.
 * 
 * If the connections fails, please refer to 
 * https://github.com/matth-x/ArduinoOcpp/issues/36#issuecomment-989716573 for recommendations on
 * how to track down the issue with the connection.
 * 
 * This is a convenience function only available for Arduino. For a full initialization with TLS,
 * please refer to https://github.com/matth-x/ArduinoOcpp/tree/master/examples/ESP-TLS
 */
void OCPP_initialize(
            const char *CS_hostname, //e.g. "example.com"
            uint16_t CS_port,        //e.g. 80
            const char *CS_url,      //e.g. "ws://example.com/steve/websocket/CentralSystemService/charger001"
            float V_eff = 230.f,     //Grid voltage of your country. e.g. 230.f (European voltage)
            ArduinoOcpp::FilesystemOpt fsOpt = ArduinoOcpp::FilesystemOpt::Use_Mount_FormatOnFail); //If this library should format the flash if necessary. Find further options in ConfigurationOptions.h
#endif

/*
 * Initialize the library with a WebSocket connection which is configured with protocol=ocpp1.6
 * (=OcppSocket), EVSE voltage and filesystem configuration. This library requires that you handle
 * establishing the connection and keeping it alive. Please refer to
 * https://github.com/matth-x/ArduinoOcpp/tree/master/examples/ESP-TLS for an example how to use it.
 * 
 * This GitHub project also delivers an OcppSocket implementation based on links2004/WebSockets. If
 * you need another WebSockets implementation, you can subclass the OcppSocket class and pass it to
 * this initialize() function. Please refer to
 * https://github.com/OpenEVSE/ESP32_WiFi_V4.x/blob/master/src/MongooseOcppSocketClient.cpp for
 * an example.
 */
void OCPP_initialize(
            ArduinoOcpp::OcppSocket& ocppSocket, //WebSocket adapter for ArduinoOcpp
            float V_eff = 230.f,                 //Grid voltage of your country. e.g. 230.f (European voltage)
            ArduinoOcpp::FilesystemOpt fsOpt = ArduinoOcpp::FilesystemOpt::Use_Mount_FormatOnFail); //If this library should format the flash if necessary. Find further options in ConfigurationOptions.h

/*
 * Stop the OCPP library and release allocated resources.
 */
void OCPP_deinitialize();

/*
 * To be called in the main loop (e.g. place it inside loop())
 */
void OCPP_loop();

/*
 * Send OCPP operations.
 * 
 * You only need to send the following operation types manually:
 *     - BootNotification: Notify the OCPP server that this EVSE is ready and start the OCPP
 *           routines.
 *     - Authorize: Validate an RFID tag before using it for a transaction
 * 
 * All other operation types are handled automatically by this library.
 * 
 * On receipt of the .conf() response the library calls the callback function
 * "OnReceiveConfListener onConf" and passes the OCPP payload to it.
 * 
 * For your first EVSE integration, the `onReceiveConfListener` is probably sufficient. For
 * advanced EVSE projects, the other listeners likely become relevant:
 * - `onAbortListener`: will be called whenever the engine stops trying to finish an operation
 *           normally which was initiated by this device.
 * - `onTimeoutListener`: will be executed when the operation is not answered until the timeout
 *           expires. Note that timeouts also trigger the `onAbortListener`.
 * - `onReceiveErrorListener`: will be called when the Central System returns a CallError.
 *           Again, each error also triggers the `onAbortListener`.
 * 
 * The functions for sending OCPP operations are non-blocking. The program will resume immediately
 * with the code after with the subsequent code in any case.
 */

void bootNotification(
            const char *chargePointModel,                //model name of the EVSE
            const char *chargePointVendor,               //vendor name
            OnReceiveConfListener onConf = nullptr,      //callback (confirmation received)
            OnAbortListener onAbort = nullptr,           //callback (confirmation not received), optional
            OnTimeoutListener onTimeout = nullptr,       //callback (timeout expired), optional
            OnReceiveErrorListener onError = nullptr,    //callback (error code received), optional
            std::unique_ptr<Timeout> timeout = nullptr); //custom timeout behavior, optional

//Alternative version for sending a complete BootNotification payload
void bootNotification(
            std::unique_ptr<DynamicJsonDocument> payload,//manually defined payload
            OnReceiveConfListener onConf = nullptr,      //callback (confirmation received)
            OnAbortListener onAbort = nullptr,           //callback (confirmation not received), optional
            OnTimeoutListener onTimeout = nullptr,       //callback (timeout expired), optional
            OnReceiveErrorListener onError = nullptr,    //callback (error code received), optional
            std::unique_ptr<Timeout> timeout = nullptr); //custom timeout behavior, optional

void authorize(
            const char *idTag,                           //RFID tag (e.g. ISO 14443 UID tag with 4 or 7 bytes)
            OnReceiveConfListener onConf = nullptr,      //callback (confirmation received)
            OnAbortListener onAbort = nullptr,           //callback (confirmation not received), optional
            OnTimeoutListener onTimeout = nullptr,       //callback (timeout expired), optional
            OnReceiveErrorListener onError = nullptr,    //callback (error code received), optional
            std::unique_ptr<Timeout> timeout = nullptr); //custom timeout behavior, optional

/*
 * Transaction management.
 * 
 * Begin the transaction process by creating a new transaction and setting its user identification.
 * The OCPP library will try to send a StartTransaction request with all data as soon as possible
 * after all requirements for a transaction are met (i.e. the connector is plugged).
 * 
 * Returns true if it was possible to create a transaction. If it returned false, either another
 * transaction is still running or you need to try it again later.
 */
bool beginTransaction(const char *idTag, unsigned int connectorId = 1);

/*
 * End the transaction process by terminating the transaction and setting a reason for its termination.
 * Please refer to to OCPP 1.6 Specification - Edition 2 p. 90 for a list of valid reasons. "reason"
 * can also be nullptr.
 * 
 * It is safe to call this function at any time, i.e. when no transaction runs or when the transaction
 * has already been ended. For example you can place `endTransaction("Reboot");` in the beginning of
 * the program just to ensure that there is no transaction from a previous run.
 * 
 * Returns true if this action actually ended a transaction. False otherwise
 */
bool endTransaction(const char *reason = nullptr, unsigned int connectorId = 1);

/*
 * Returns if the library has started the transaction by sending a StartTransaction and if it hasn't
 * been stopped already by sending a StopTransaction.
 */
bool isTransactionRunning(unsigned int connectorId = 1);

/* 
 * Returns if the OCPP library allows the EVSE to charge at the moment.
 *
 * If you integrate it into a J1772 charger, true means that the Control Pilot can send the PWM signal
 * and false means that the Control Pilot must be at a DC voltage.
 */
bool ocppPermitsCharge(unsigned int connectorId = 1);

/*
 * Define the Inputs and Outputs of this library.
 * 
 * This library interacts with the hardware of your charger by Inputs and Outputs. Inputs and Outputs
 * are tiny function-objects which read information from the EVSE or control the behavior of the EVSE.
 * 
 * An Input is a function which returns the current state of a variable of the EVSE. For example, if
 * the energy meter stores the energy register in the global variable `e_reg`, then you can allow
 * this library to read it by defining the Input 
 *     `[] () {return e_reg;}`
 * and passing it to the library.
 * 
 * An Output is a function which gets a state value from the OCPP library and applies it to the EVSE.
 * For example, to let Smart Charging control the PWM signal of the Control Pilot, define the Output
 *     `[] (float p_max) {pwm = p_max / PWM_FACTOR;}` (simplified example)
 * and pass it to the library.
 * 
 * Configure the library with Inputs and Outputs once in the setup() function.
 */

void setConnectorPluggedInput(std::function<bool()> pluggedInput, unsigned int connectorId = 1); //Input about if an EV is plugged to this EVSE

void setEnergyMeterInput(std::function<float()> energyInput, unsigned int connectorId = 1); //Input of the electricity meter register

void setPowerMeterInput(std::function<float()> powerInput, unsigned int connectorId = 1); //Input of the power meter reading

void setSmartChargingOutput(std::function<void(float)> chargingLimitOutput, unsigned int connectorId = 1); //Output for the Smart Charging limit

/*
 * Define the Inputs and Outputs of this library. (Advanced)
 * 
 * These Inputs and Outputs are optional depending on the use case of your charger.
 */

void setEvReadyInput(std::function<bool()> evReadyInput, unsigned int connectorId = 1); //Input if EV is ready to charge (= J1772 State C)

void setEvseReadyInput(std::function<bool()> evseReadyInput, unsigned int connectorId = 1); //Input if EVSE allows charge (= PWM signal on)

void addErrorCodeInput(std::function<const char *()> errorCodeInput, unsigned int connectorId = 1); //Input for Error codes (please refer to OCPP 1.6, Edit2, p. 71 and 72 for valid error codes)

void addMeterValueInput(std::function<int32_t ()> valueInput, const char *measurand = nullptr, const char *unit = nullptr, const char *location = nullptr, const char *phase = nullptr, unsigned int connectorId = 1); //integrate further metering Inputs

void addMeterValueInput(std::unique_ptr<ArduinoOcpp::SampledValueSampler> valueInput, unsigned int connectorId = 1); //integrate further metering Inputs (more extensive alternative)

void setOnResetNotify(std::function<bool(bool)> onResetNotify); //call onResetNotify(isHard) before Reset. If you return false, Reset will be aborted. Optional

void setOnResetExecute(std::function<void(bool)> onResetExecute); //reset handler. This function should reboot this controller immediately. Already defined for the ESP32 on Arduino

/*
 * Set an InputOutput (reads and sets information at the same time) for forcing to unlock the
 * connector. Called as part of the OCPP operation "UnlockConnector"
 * Return values: true on success, false on failure, PollResult::Await if not known yet
 * Continues to call the Cb as long as it returns PollResult::Await
 */
void setOnUnlockConnectorInOut(std::function<ArduinoOcpp::PollResult<bool>()> onUnlockConnectorInOut, unsigned int connectorId = 1);

/*
 * Set an Input/Output for setting the state of the connector lock. Called in the course of normal
 * transactions (not as part of UnlockConnector).
 * Param. values: 
 *     - TxTrigger::Active if connector should be locked
 *     - TxTrigger::Inactive if connector should be unlocked
 * Return values:
 *     - TxEnableState::Active if connector is locked and ready for transaction
 *     - TxEnableState::Inactive if connector lock is released
 *     - TxEnableState::Pending otherwise, e.g. if transitioning between the states
 */
void setConnectorLockInOut(std::function<ArduinoOcpp::TxEnableState(ArduinoOcpp::TxTrigger)> lockConnectorInOut, unsigned int connectorId = 1);

/*
 * Set an Input/Output to interact with a transaction-based energy meter. When this OCPP library is
 * about to start a transaction, it toggles the Output to the energy meter to TxTrigger::Active so
 * the energy meter can take a measurement right before a transaction. The same goes for the stop of
 * transactions. With the Input, the energy meter signals if the measurement is ready and the
 * transaction can finally start / stop.
 * Param. values:
 *     - TxTrigger::Active if the tx-based meter should be in transaction-mode
 *     - TxTrigger::Inactive if the tx-based meter should be in non-transaction-mode
 * Return values:
 *     - TxEnableState::Active if the tx-based meter confirms to be in the transaction-mode
 *     - TxEnableState::Inactive if the tx-based meter has transitioned into a non-transaction-mode
 *     - TxEnableState::Pending otherwise, e.g. if transitioning between the states
 */
void setTxBasedMeterInOut(std::function<ArduinoOcpp::TxEnableState(ArduinoOcpp::TxTrigger)> txMeterInOut, unsigned int connectorId = 1);

/*
 * Access further information about the internal state of the library
 */

bool isOperative(unsigned int connectorId = 1); //if the charge point is operative or inoperative (see OCPP1.6 Edit2, p. 45)

int getTransactionId(unsigned int connectorId = 1); //returns the txId if known, -1 if no transaction is running and 0 if txId not assigned yet

const char *getTransactionIdTag(unsigned int connectorId = 1); //returns the authorization token if applicable, or nullptr otherwise

bool isBlockedByReservation(const char *idTag, unsigned int connectorId = 1); //if the connector is already reserved for a different idTag

void setOnResetRequest(OnReceiveReqListener onReceiveReq);

/*
 * Configure the device management
 */

#if defined(AO_CUSTOM_UPDATER) || defined(AO_CUSTOM_WS)
#include <ArduinoOcpp/Tasks/FirmwareManagement/FirmwareService.h>

/*
 * You need to configure this object if FW updates are relevant for you. This project already
 * brings a simple configuration for the ESP32 and ESP8266 for prototyping purposes, however
 * for the productive system you will have to develop a configuration targeting the specific
 * OCPP backend.
 * See ArduinoOcpp/Tasks/FirmwareManagement/FirmwareService.h 
 */
ArduinoOcpp::FirmwareService *getFirmwareService();
#endif

#if defined(AO_CUSTOM_DIAGNOSTICS) || defined(AO_CUSTOM_WS)
#include <ArduinoOcpp/Tasks/Diagnostics/DiagnosticsService.h>
/*
 * This library implements the OCPP messaging side of Diagnostics, but no logging or the
 * log upload to your backend.
 * To integrate Diagnostics, see ArduinoOcpp/Tasks/Diagnostics/DiagnosticsService.h
 */
ArduinoOcpp::DiagnosticsService *getDiagnosticsService();
#endif

namespace ArduinoOcpp {
class OcppEngine;
}

//Get access to internal functions and data structures. The returned OcppEngine object allows
//you to bypass the facade functions of this header and implement custom functionality.
ArduinoOcpp::OcppEngine *getOcppEngine();

/*
 * Deprecated functions or functions to be moved to ArduinoOcppExtended.h
 */

void setOnSetChargingProfileRequest(OnReceiveReqListener onReceiveReq); //optional

void setOnRemoteStartTransactionSendConf(OnSendConfListener onSendConf);

void setOnRemoteStopTransactionSendConf(OnSendConfListener onSendConf);
void setOnRemoteStopTransactionReceiveReq(OnReceiveReqListener onReceiveReq);

void setOnResetSendConf(OnSendConfListener onSendConf);

bool startTransaction(const char *idTag, OnReceiveConfListener onConf = nullptr, OnAbortListener onAbort = nullptr, OnTimeoutListener onTimeout = nullptr, OnReceiveErrorListener onError = nullptr, std::unique_ptr<Timeout> timeout = nullptr);

bool stopTransaction(OnReceiveConfListener onConf = nullptr, OnAbortListener onAbort = nullptr, OnTimeoutListener onTimeout = nullptr, OnReceiveErrorListener onError = nullptr, std::unique_ptr<Timeout> timeout = nullptr);

#endif
