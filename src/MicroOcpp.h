// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef ARDUINOOCPP_H
#define ARDUINOOCPP_H

#include <ArduinoJson.h>
#include <memory>
#include <functional>

#include <MicroOcpp/Core/ConfigurationOptions.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/RequestCallbacks.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/PollResult.h>
#include <MicroOcpp/Model/Metering/SampledValue.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Model/ConnectorBase/Notification.h>
#include <MicroOcpp/Model/ConnectorBase/ChargePointErrorData.h>

using MicroOcpp::OnReceiveConfListener;
using MicroOcpp::OnReceiveReqListener;
using MicroOcpp::OnSendConfListener;
using MicroOcpp::OnAbortListener;
using MicroOcpp::OnTimeoutListener;
using MicroOcpp::OnReceiveErrorListener;

#ifndef MO_CUSTOM_WS
//use links2004/WebSockets library

/*
 * Initialize the library with the OCPP URL, EVSE voltage and filesystem configuration.
 * 
 * If the connections fails, please refer to 
 * https://github.com/matth-x/MicroOcpp/issues/36#issuecomment-989716573 for recommendations on
 * how to track down the issue with the connection.
 * 
 * This is a convenience function only available for Arduino.
 */
void mocpp_initialize(
            const char *backendUrl,    //e.g. "wss://example.com:8443/steve/websocket/CentralSystemService"
            const char *chargeBoxId,   //e.g. "charger001"
            const char *chargePointModel = "Demo Charger",     //model name of this charger
            const char *chargePointVendor = "My Company Ltd.", //brand name
            MicroOcpp::FilesystemOpt fsOpt = MicroOcpp::FilesystemOpt::Use_Mount_FormatOnFail, //If this library should format the flash if necessary. Find further options in ConfigurationOptions.h
            const char *password = nullptr, //password present in the websocket message header
            const char *CA_cert = nullptr, //TLS certificate
            bool autoRecover = false); //automatically sanitize the local data store when the lib detects recurring crashes. Not recommended during development
#endif

/*
 * Convenience initialization: use this for passing the BootNotification payload JSON to the mocpp_initialize(...) below
 *
 * Example usage:
 * 
 *     mocpp_initialize(osock, ChargerCredentials("Demo Charger", "My Company Ltd."));
 * 
 * For a description of the fields, refer to OCPP 1.6 Specification - Edition 2 p. 60
 */
struct ChargerCredentials {
    ChargerCredentials(
            const char *chargePointModel = "Demo Charger",
            const char *chargePointVendor = "My Company Ltd.",
            const char *firmwareVersion = nullptr,
            const char *chargePointSerialNumber = nullptr,
            const char *meterSerialNumber = nullptr,
            const char *meterType = nullptr,
            const char *chargeBoxSerialNumber = nullptr,
            const char *iccid = nullptr,
            const char *imsi = nullptr);

    operator const char *() {return payload;}

private:
    char payload [512] = {'{', '}', '\0'};
};

/*
 * Initialize the library with a WebSocket connection which is configured with protocol=ocpp1.6
 * (=Connection), EVSE voltage and filesystem configuration. This library requires that you handle
 * establishing the connection and keeping it alive. Please refer to
 * https://github.com/matth-x/MicroOcpp/tree/master/examples/ESP-TLS for an example how to use it.
 * 
 * This GitHub project also delivers an Connection implementation based on links2004/WebSockets. If
 * you need another WebSockets implementation, you can subclass the Connection class and pass it to
 * this initialize() function. Please refer to
 * https://github.com/OpenEVSE/ESP32_WiFi_V4.x/blob/master/src/MongooseConnectionClient.cpp for
 * an example.
 */
void mocpp_initialize(
            MicroOcpp::Connection& connection, //WebSocket adapter for MicroOcpp
            const char *bootNotificationCredentials = ChargerCredentials("Demo Charger", "My Company Ltd."), //e.g. '{"chargePointModel":"Demo Charger","chargePointVendor":"My Company Ltd."}' (refer to OCPP 1.6 Specification - Edition 2 p. 60)
            std::shared_ptr<MicroOcpp::FilesystemAdapter> filesystem =
                MicroOcpp::makeDefaultFilesystemAdapter(MicroOcpp::FilesystemOpt::Use_Mount_FormatOnFail), //If this library should format the flash if necessary. Find further options in ConfigurationOptions.h
            bool autoRecover = false); //automatically sanitize the local data store when the lib detects recurring crashes. Not recommended during development

/*
 * Stop the OCPP library and release allocated resources.
 */
void mocpp_deinitialize();

/*
 * To be called in the main loop (e.g. place it inside loop())
 */
void mocpp_loop();

/*
 * Transaction management.
 * 
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
 * Returns the transaction object if it was possible to create the transaction process. Returns
 * nullptr if either another transaction process is still active or you need to try it again later.
 */
std::shared_ptr<MicroOcpp::Transaction> beginTransaction(const char *idTag, unsigned int connectorId = 1);

/*
 * Begin the transaction process and skip the OCPP-side authorization. See beginTransaction(...) for a
 * complete description
 */
std::shared_ptr<MicroOcpp::Transaction> beginTransaction_authorized(const char *idTag, const char *parentIdTag = nullptr, unsigned int connectorId = 1);

/*
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
 */
bool endTransaction(const char *idTag = nullptr, const char *reason = nullptr, unsigned int connectorId = 1);

/*
 * End the transaction process definitely without authorization check. See endTransaction(...) for a
 * complete description.
 * 
 * Use this function if you manage authorization on your own and want to bypass the Authorization
 * management of this lib.
 */
bool endTransaction_authorized(const char *idTag, const char *reason = nullptr, unsigned int connectorId = 1);

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
bool isTransactionActive(unsigned int connectorId = 1);
bool isTransactionRunning(unsigned int connectorId = 1);

/*
 * Get the idTag which has been used to start the transaction. If no transaction process is
 * running, this function returns nullptr
 */
const char *getTransactionIdTag(unsigned int connectorId = 1);

/*
 * Returns the current transaction process. Returns nullptr if no transaction is running, preparing or finishing
 *
 * See the class definition in MicroOcpp/Model/Transactions/Transaction.h for possible uses of this object
 * 
 * Examples:
 * auto tx = getTransaction(); //fetch tx object
 * if (tx) { //check if tx object exists
 *     bool active = tx->isActive(); //active tells if the transaction is preparing or continuing to run
 *                                   //inactive means that the transaction is about to stop, stopped or won't be started anymore
 *     int transactionId = tx->getTransactionId(); //the transactionId as assigned by the OCPP server
 *     bool deauthorized = tx->isIdTagDeauthorized(); //if StartTransaction has been rejected
 * }
 */
std::shared_ptr<MicroOcpp::Transaction>& getTransaction(unsigned int connectorId = 1);

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

//Smart Charging Output, alternative for Watts only, Current only, or Watts x Current x numberPhases. Only one
//of them can be set at a time
void setSmartChargingPowerOutput(std::function<void(float)> chargingLimitOutput, unsigned int connectorId = 1); //Output (in Watts) for the Smart Charging limit
void setSmartChargingCurrentOutput(std::function<void(float)> chargingLimitOutput, unsigned int connectorId = 1); //Output (in Amps) for the Smart Charging limit
void setSmartChargingOutput(std::function<void(float,float,int)> chargingLimitOutput, unsigned int connectorId = 1); //Output (in Watts, Amps, numberPhases) for the Smart Charging limit

/*
 * Define the Inputs and Outputs of this library. (Advanced)
 * 
 * These Inputs and Outputs are optional depending on the use case of your charger.
 */

void setEvReadyInput(std::function<bool()> evReadyInput, unsigned int connectorId = 1); //Input if EV is ready to charge (= J1772 State C)

void setEvseReadyInput(std::function<bool()> evseReadyInput, unsigned int connectorId = 1); //Input if EVSE allows charge (= PWM signal on)

void addErrorCodeInput(std::function<const char*()> errorCodeInput, unsigned int connectorId = 1); //Input for Error codes (please refer to OCPP 1.6, Edit2, p. 71 and 72 for valid error codes)
void addErrorDataInput(std::function<MicroOcpp::ErrorData()> errorDataInput, unsigned int connectorId = 1);

void addMeterValueInput(std::function<float ()> valueInput, const char *measurand = nullptr, const char *unit = nullptr, const char *location = nullptr, const char *phase = nullptr, unsigned int connectorId = 1); //integrate further metering Inputs

void addMeterValueInput(std::unique_ptr<MicroOcpp::SampledValueSampler> valueInput, unsigned int connectorId = 1); //integrate further metering Inputs (more extensive alternative)

void setOccupiedInput(std::function<bool()> occupied, unsigned int connectorId = 1); //Input if instead of Available, send StatusNotification Preparing / Finishing

void setStartTxReadyInput(std::function<bool()> startTxReady, unsigned int connectorId = 1); //Input if the charger is ready for StartTransaction

void setStopTxReadyInput(std::function<bool()> stopTxReady, unsigned int connectorId = 1); //Input if charger is ready for StopTransaction

void setTxNotificationOutput(std::function<void(MicroOcpp::Transaction*,MicroOcpp::TxNotification)> notificationOutput, unsigned int connectorId = 1); //called when transaction state changes (see TxNotification for possible events). Transaction can be null

/*
 * Set an InputOutput (reads and sets information at the same time) for forcing to unlock the
 * connector. Called as part of the OCPP operation "UnlockConnector"
 * Return values: true on success, false on failure, PollResult::Await if not known yet
 * Continues to call the Cb as long as it returns PollResult::Await
 */
void setOnUnlockConnectorInOut(std::function<MicroOcpp::PollResult<bool>()> onUnlockConnectorInOut, unsigned int connectorId = 1);

/*
 * Access further information about the internal state of the library
 */

bool isOperative(unsigned int connectorId = 1); //if the charge point is operative (see OCPP1.6 Edit2, p. 45) and ready for transactions

/*
 * Configure the device management
 */

void setOnResetNotify(std::function<bool(bool)> onResetNotify); //call onResetNotify(isHard) before Reset. If you return false, Reset will be aborted. Optional

void setOnResetExecute(std::function<void(bool)> onResetExecute); //reset handler. This function should reboot this controller immediately. Already defined for the ESP32 on Arduino

#if defined(MO_CUSTOM_UPDATER) || defined(MO_CUSTOM_WS)
#include <MicroOcpp/Model/FirmwareManagement/FirmwareService.h>

/*
 * You need to configure this object if FW updates are relevant for you. This project already
 * brings a simple configuration for the ESP32 and ESP8266 for prototyping purposes, however
 * for the productive system you will have to develop a configuration targeting the specific
 * OCPP backend.
 * See MicroOcpp/Model/FirmwareManagement/FirmwareService.h 
 */
MicroOcpp::FirmwareService *getFirmwareService();
#endif

#if defined(MO_CUSTOM_DIAGNOSTICS) || defined(MO_CUSTOM_WS)
#include <MicroOcpp/Model/Diagnostics/DiagnosticsService.h>
/*
 * This library implements the OCPP messaging side of Diagnostics, but no logging or the
 * log upload to your backend.
 * To integrate Diagnostics, see MicroOcpp/Model/Diagnostics/DiagnosticsService.h
 */
MicroOcpp::DiagnosticsService *getDiagnosticsService();
#endif

/*
 * Add features and customize the behavior of the OCPP client
 */

namespace MicroOcpp {
class Context;
}

//Get access to internal functions and data structures. The returned Context object allows
//you to bypass the facade functions of this header and implement custom functionality.
MicroOcpp::Context *getOcppContext();

/*
 * Set a listener which is notified when the OCPP lib processes an incoming operation of type
 * operationType. After the operation has been interpreted, onReceiveReq will be called with
 * the original message from the OCPP server.
 * 
 * Example usage:
 * 
 * setOnReceiveRequest("SetChargingProfile", [] (JsonObject payload) {
 *     Serial.print("[main] received charging profile for connector: "; //Arduino print function
 *     Serial.printf("update connector %i with chargingProfileId %i\n",
 *             payload["connectorId"],                                  //ArduinoJson object access
 *             payload["csChargingProfiles"]["chargingProfileId"]);
 * });
 */
void setOnReceiveRequest(const char *operationType, OnReceiveReqListener onReceiveReq);

/*
 * Set a listener which is notified when the OCPP lib sends the confirmation to an incoming
 * operation of type operation type. onSendConf will be passed the original output of the
 * OCPP lib.
 * 
 * Example usage:
 * 
 * setOnSendConf("RemoteStopTransaction", [] (JsonObject payload) -> void {
 *     if (!strcmp(payload["status"], "Rejected")) {
 *         //the OCPP lib rejected the RemoteStopTransaction command. In this example, the customer
 *         //wishes to stop the running transaction in any case and to log this case
 *         endTransaction(nullptr, "Remote"); //end transaction and send StopTransaction
 *         Serial.println("[main] override rejected RemoteStopTransaction"); //Arduino print function
 *     }
 * });
 * 
 */
void setOnSendConf(const char *operationType, OnSendConfListener onSendConf);

/*
 * Create and send an operation without using the built-in Operation class. This function bypasses
 * the business logic which comes with this library. E.g. you can send unknown operations, extend
 * OCPP or replace parts of the business logic with custom behavior.
 * 
 * Use case 1, extend the library by sending additional operations. E.g. DataTransfer:
 * 
 * sendRequest("DataTransfer", [] () -> std::unique_ptr<DynamicJsonDocument> {
 *     //will be called to create the request once this operation is being sent out
 *     size_t capacity = JSON_OBJECT_SIZE(3) +
 *                       JSON_OBJECT_SIZE(2); //for calculating the required capacity, see https://arduinojson.org/v6/assistant/
 *     auto res = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(capacity)); 
 *     JsonObject request = *res;
 *     request["vendorId"] = "My company Ltd.";
 *     request["messageId"] = "TargetValues";
 *     request["data"]["battery_capacity"] = 89;
 *     request["data"]["battery_soc"] = 34;
 *     return res;
 * }, [] (JsonObject response) -> void {
 *     //will be called with the confirmation response of the server
 *     if (!strcmp(response["status"], "Accepted")) {
 *         //DataTransfer has been accepted
 *         int max_energy = response["data"]["max_energy"];
 *     }
 * });
 * 
 * Use case 2, bypass the business logic of this library for custom behavior. E.g. StartTransaction:
 * 
 * sendRequest("StartTransaction", [] () -> std::unique_ptr<DynamicJsonDocument> {
 *     //will be called to create the request once this operation is being sent out
 *     size_t capacity = JSON_OBJECT_SIZE(4); //for calculating the required capacity, see https://arduinojson.org/v6/assistant/
 *     auto res = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(capacity)); 
 *     JsonObject request = res->to<JsonObject>();
 *     request["connectorId"] = 1;
 *     request["idTag"] = "A9C3CE1D7B71EA";
 *     request["meterStart"] = 1234;
 *     request["timestamp"] = "2023-06-01T11:07:43Z"; //e.g. some historic transaction
 *     return res;
 * }, [] (JsonObject response) -> void {
 *     //will be called with the confirmation response of the server
 *     const char *status = response["idTagInfo"]["status"];
 *     int transactionId = response["transactionId"];
 * });
 * 
 * In Use case 2, the library won't send any further StatusNotification or StopTransaction on
 * its own.
 */
void sendRequest(const char *operationType,
            std::function<std::unique_ptr<DynamicJsonDocument> ()> fn_createReq,
            std::function<void (JsonObject)> fn_processConf);

/*
 * Set a custom handler for an incoming operation type. This will update the core Operation registry
 * of the library, potentially replacing the built-in Operation handler and bypassing the
 * business logic of this library.
 * 
 * Note that when replacing an operation handler, the attached listeners will be reset.
 * 
 * Example usage:
 * 
 * setRequestHandler("DataTransfer", [] (JsonObject request) -> void {
 *     //will be called with the request message from the server
 *     const char *vendorId = request["vendorId"];
 *     const char *messageId = request["messageId"];
 *     int battery_capacity = request["data"]["battery_capacity"];
 *     int battery_soc = request["data"]["battery_soc"];
 * }, [] () -> std::unique_ptr<DynamicJsonDocument> {
 *     //will be called  to create the response once this operation is being sent out
 *     size_t capacity = JSON_OBJECT_SIZE(2) +
 *                       JSON_OBJECT_SIZE(1); //for calculating the required capacity, see https://arduinojson.org/v6/assistant/
 *     auto res = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(capacity)); 
 *     JsonObject response = res->to<JsonObject>();
 *     response["status"] = "Accepted";
 *     response["data"]["max_energy"] = 59;
 *     return res;
 * });
 */
void setRequestHandler(const char *operationType,
            std::function<void (JsonObject)> fn_processReq,
            std::function<std::unique_ptr<DynamicJsonDocument> ()> fn_createConf);

/*
 * Send OCPP operations manually not bypassing the internal business logic
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

void authorize(
            const char *idTag,                           //RFID tag (e.g. ISO 14443 UID tag with 4 or 7 bytes)
            OnReceiveConfListener onConf = nullptr,      //callback (confirmation received)
            OnAbortListener onAbort = nullptr,           //callback (confirmation not received), optional
            OnTimeoutListener onTimeout = nullptr,       //callback (timeout expired), optional
            OnReceiveErrorListener onError = nullptr,    //callback (error code received), optional
            unsigned int timeout = 0); //custom timeout behavior, optional

bool startTransaction(
            const char *idTag,
            OnReceiveConfListener onConf = nullptr,
            OnAbortListener onAbort = nullptr,
            OnTimeoutListener onTimeout = nullptr,
            OnReceiveErrorListener onError = nullptr,
            unsigned int timeout = 0);

bool stopTransaction(
            OnReceiveConfListener onConf = nullptr,
            OnAbortListener onAbort = nullptr,
            OnTimeoutListener onTimeout = nullptr,
            OnReceiveErrorListener onError = nullptr,
            unsigned int timeout = 0);

#endif
