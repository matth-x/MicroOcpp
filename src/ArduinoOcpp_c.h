// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef ARDUINOOCPP_C_H
#define ARDUINOOCPP_C_H

#include <stddef.h>

#include <ArduinoOcpp/Core/ConfigurationOptions.h>
#include <ArduinoOcpp/Model/ChargeControl/Notification.h>
#include <ArduinoOcpp/Model/Transactions/Transaction.h>

struct AO_Connection;
typedef struct AO_Connection AO_Connection;

struct MeterValueInput;
typedef struct MeterValueInput MeterValueInput;

typedef void (*OnMessage) (const char *payload, size_t len);
typedef void (*OnAbort)   ();
typedef void (*OnTimeout) ();
typedef void (*OnCallError)   (const char *code, const char *description, const char *details_json, size_t details_len);
typedef void (*AuthorizeConfCallback)    (const char *idTag, const char *payload, size_t len, void *user_data);
typedef void (*AuthorizeAbortCallback)   (const char *idTag, void* user_data);
typedef void (*AuthorizeTimeoutCallback) (const char *idTag, void* user_data);
typedef void (*AuthorizeErrorCallback)   (const char *idTag, const char *code, const char *description, const char *details_json, size_t details_len, void* user_data);

typedef float (*InputFloat)();
typedef float (*InputFloat_m)(unsigned int connectorId); //multiple connectors version
typedef int   (*InputInt)();
typedef int   (*InputInt_m)(unsigned int connectorId);
typedef bool  (*InputBool)();
typedef bool  (*InputBool_m)(unsigned int connectorId);
typedef const char* (*InputString)();
typedef const char* (*InputString_m)(unsigned int connectorId);
typedef void (*OutputFloat)(float limit);
typedef void (*OutputFloat_m)(unsigned int connectorId, float limit);
typedef void (*OutputSmartCharging)(float power, float current, int nphases);
typedef void (*OutputSmartCharging_m)(unsigned int connectorId, float power, float current, int nphases);
enum OptionalBool {OptionalTrue, OptionalFalse, OptionalNone};
typedef enum OptionalBool (*PollBool)();
typedef enum OptionalBool (*PollBool_m)(unsigned int connectorId);


#ifdef __cplusplus
extern "C" {
#endif

/*
 * Please refer to ArduinoOcpp.h for the documentation
 */

void ao_initialize(
            AO_Connection *conn,  //WebSocket adapter for ArduinoOcpp
            const char *chargePointModel,     //model name of this charger (e.g. "")
            const char *chargePointVendor, //brand name
            struct AO_FilesystemOpt fsopt); //If this library should format the flash if necessary. Find further options in ConfigurationOptions.h

//same as above, but more fields for the BootNotification
void ao_initialize_full(
            AO_Connection *conn,  //WebSocket adapter for ArduinoOcpp
            const char *bootNotificationCredentials, //e.g. '{"chargePointModel":"Demo Charger","chargePointVendor":"My Company Ltd."}' (refer to OCPP 1.6 Specification - Edition 2 p. 60)
            struct AO_FilesystemOpt fsopt); //If this library should format the flash if necessary. Find further options in ConfigurationOptions.h


void ao_deinitialize();

void ao_loop();

/*
 * Charging session management
 */

void ao_beginTransaction(const char *idTag);
void ao_beginTransaction_m(unsigned int connectorId, const char *idTag); //multiple connectors version

void ao_beginTransaction_authorized(const char *idTag, const char *parentIdTag);
void ao_beginTransaction_authorized_m(unsigned int connectorId, const char *idTag, const char *parentIdTag);

bool ao_endTransaction(const char *idTag, const char *reason); //idTag, reason can be NULL
bool ao_endTransaction_m(unsigned int connectorId, const char *idTag, const char *reason); //idTag, reason can be NULL

bool ao_isTransactionActive();
bool ao_isTransactionActive_m(unsigned int connectorId);

bool ao_isTransactionRunning();
bool ao_isTransactionRunning_m(unsigned int connectorId);

const char *ao_getTransactionIdTag();
const char *ao_getTransactionIdTag_m(unsigned int connectorId);

AO_Transaction *ao_getTransaction();
AO_Transaction *ao_getTransaction_m(unsigned int connectorId);

bool ao_ocppPermitsCharge();
bool ao_ocppPermitsCharge_m(unsigned int connectorId);

/*
 * Define the Inputs and Outputs of this library.
 */

void ao_setConnectorPluggedInput(InputBool pluggedInput);
void ao_setConnectorPluggedInput_m(unsigned int connectorId, InputBool_m pluggedInput);

void ao_setEnergyMeterInput(InputInt energyInput);
void ao_setEnergyMeterInput_m(unsigned int connectorId, InputInt_m energyInput);

void ao_setPowerMeterInput(InputFloat powerInput);
void ao_setPowerMeterInput_m(unsigned int connectorId, InputFloat_m powerInput);

void ao_setSmartChargingPowerOutput(OutputFloat maxPowerOutput);
void ao_setSmartChargingPowerOutput_m(unsigned int connectorId, OutputFloat_m maxPowerOutput);
void ao_setSmartChargingCurrentOutput(OutputFloat maxCurrentOutput);
void ao_setSmartChargingCurrentOutput_m(unsigned int connectorId, OutputFloat_m maxCurrentOutput);
void ao_setSmartChargingOutput(OutputSmartCharging chargingLimitOutput);
void ao_setSmartChargingOutput_m(unsigned int connectorId, OutputSmartCharging_m chargingLimitOutput);

/*
 * Define the Inputs and Outputs of this library. (Advanced)
 */

void ao_setEvReadyInput(InputBool evReadyInput);
void ao_setEvReadyInput_m(unsigned int connectorId, InputBool_m evReadyInput);

void ao_setEvseReadyInput(InputBool evseReadyInput);
void ao_setEvseReadyInput_m(unsigned int connectorId, InputBool_m evseReadyInput);

void ao_addErrorCodeInput(InputString errorCodeInput);
void ao_addErrorCodeInput_m(unsigned int connectorId, InputString_m errorCodeInput);

void ao_addMeterValueInputInt(InputInt valueInput, const char *measurand, const char *unit, const char *location, const char *phase); //measurand, unit, location and phase can be NULL
void ao_addMeterValueInputInt_m(unsigned int connectorId, InputInt_m valueInput, const char *measurand, const char *unit, const char *location, const char *phase); //measurand, unit, location and phase can be NULL

void ao_addMeterValueInput(MeterValueInput *meterValueInput); //takes ownership of meterValueInput
void ao_addMeterValueInput_m(unsigned int connectorId, MeterValueInput *meterValueInput); //takes ownership of meterValueInput

void ao_setOccupiedInput(InputBool occupied);
void ao_setOccupiedInput_m(unsigned int connectorId, InputBool_m occupied);

void ao_setStartTxReadyInput(InputBool startTxReady);
void ao_setStartTxReadyInput_m(unsigned int connectorId, InputBool_m startTxReady);

void ao_setStopTxReadyInput(InputBool stopTxReady);
void ao_setStopTxReadyInput_m(unsigned int connectorId, InputBool_m stopTxReady);

void ao_setTxNotificationOutput(void (*notificationOutput)(enum AO_TxNotification, AO_Transaction*));
void ao_setTxNotificationOutput_m(unsigned int connectorId, void (*notificationOutput)(unsigned int, enum AO_TxNotification, AO_Transaction*));

void ao_setOnUnlockConnectorInOut(PollBool onUnlockConnectorInOut);
void ao_setOnUnlockConnectorInOut_m(unsigned int connectorId, PollBool_m onUnlockConnectorInOut);

/*
 * Access further information about the internal state of the library
 */

bool ao_isOperative();
bool ao_isOperative_m(unsigned int connectorId);

void ao_setOnResetNotify(bool (*onResetNotify)(bool));

void ao_setOnResetExecute(void (*onResetExecute)(bool));

void ao_setOnReceiveRequest(const char *operationType, OnMessage onRequest);

void ao_setOnSendConf(const char *operationType, OnMessage onConfirmation);

/*
 * If build flag AO_CUSTOM_CONSOLE is set, all console output will be forwarded to the print
 * function given by this setter. The parameter msg will also by null-terminated c-strings.
 */
void ao_set_console_out_c(void (*console_out)(const char *msg));

/*
 * Send OCPP operations
 */
void ao_authorize(const char *idTag, AuthorizeConfCallback onConfirmation, AuthorizeAbortCallback onAbort, AuthorizeTimeoutCallback onTimeout, AuthorizeErrorCallback onError, void *user_data);

void ao_startTransaction(const char *idTag, OnMessage onConfirmation, OnAbort onAbort, OnTimeout onTimeout, OnCallError onError);

void ao_stopTransaction(OnMessage onConfirmation, OnAbort onAbort, OnTimeout onTimeout, OnCallError onError);

#ifdef __cplusplus
}
#endif

#endif
