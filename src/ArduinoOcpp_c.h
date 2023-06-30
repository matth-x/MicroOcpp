#ifndef ARDUINOOCPP_C_H
#define ARDUINOOCPP_C_H

#include <stddef.h>
#include <ArduinoOcpp/Core/ConfigurationOptions.h>
#include <ArduinoOcpp/Model/ChargeControl/ChargePointErrorData.h>

struct AConnection;
typedef struct AConnection AConnection;

struct MeterValueInput;
typedef struct MeterValueInput MeterValueInput;

struct OcppHandle;
typedef struct OcppHandle OcppHandle;

typedef void (*OnMessage) (const char *payload, size_t len);
typedef void (*OnAbort)   ();
typedef void (*OnTimeout) ();
typedef void (*OnCallError)   (const char *code, const char *description, const char *details_json, size_t details_len);
//typedef void (*ConfCallback)    (const char *payload, size_t len, void* user_data);
//typedef void (*AbortCallback)   (void* user_data);
//typedef void (*TimeoutCallback) (void* user_data);
//typedef void (*ErrorCallback)   (const char *code, const char *description, const char *details_json, size_t details_len, void* user_data);
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
            AConnection *osock,  //WebSocket adapter for ArduinoOcpp
            const char *chargePointModel,     //model name of this charger (e.g. "")
            const char *chargePointVendor, //brand name
            struct AO_FilesystemOpt fsopt); //If this library should format the flash if necessary. Find further options in ConfigurationOptions.h

//same as above, but more fields for the BootNotification
void ao_initialize_full(
            AConnection *osock,  //WebSocket adapter for ArduinoOcpp
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

bool ao_isTransactionRunning();
bool ao_isTransactionRunning_m(unsigned int connectorId);

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

void ao_setSmartChargingOutput(OutputFloat chargingLimitOutput);
void ao_setSmartChargingOutput_m(unsigned int connectorId, OutputFloat_m chargingLimitOutput);

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

void ao_setOnUnlockConnectorInOut(PollBool onUnlockConnectorInOut);
void ao_setOnUnlockConnectorInOut_m(unsigned int connectorId, PollBool_m onUnlockConnectorInOut);

void ao_setStartTxReadyInput(InputBool startTxReady);
void ao_setStartTxReadyInput_m(unsigned int connectorId, InputBool_m startTxReady);

void ao_setStartTxReadyInput(InputBool startTxReady);
void ao_setStartTxReadyInput_m(unsigned int connectorId, InputBool_m startTxReady);

void ao_setOccupiedInput(InputBool occupied);
void ao_setOccupiedInput_m(unsigned int connectorId, InputBool_m occupied);

/*
 * Access further information about the internal state of the library
 */

bool ao_isOperative();
bool ao_isOperative_m(unsigned int connectorId);

int ao_getTransactionId();
int ao_getTransactionId_m(unsigned int connectorId);

const char *ao_getTransactionIdTag();
const char *ao_getTransactionIdTag_m(unsigned int connectorId);

void ao_setOnReceiveRequest(const char *operationType, OnMessage onRequest);

void ao_setOnSendConf(const char *operationType, OnMessage onConfirmation);

/*
 * If build flag AO_CUSTOM_CONSOLE is set, all console output will be forwarded to the print
 * function given by this setter. The parameter msg will also by null-terminated c-strings.
 */
void ao_set_console_out_c(void (*console_out)(const char *msg));

/*
 * Get access to the internals
 */

OcppHandle *ao_getOcppHandle();

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
