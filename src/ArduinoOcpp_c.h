#ifndef ARDUINOOCPP_C_H
#define ARDUINOOCPP_C_H

#include <stddef.h>
#include <ArduinoOcpp/Core/ConfigurationOptions.h>

struct AOcppSocket;
typedef struct AOcppSocket AOcppSocket;

struct MeterValueInput;
typedef struct MeterValueInput MeterValueInput;

struct OcppHandle;
typedef struct OcppHandle OcppHandle;

typedef void (*OnOcppMessage) (const char *payload, size_t len);
typedef void (*OnOcppAbort)   ();
typedef void (*OnOcppTimeout) ();
typedef void (*OnOcppError)   (const char *code, const char *description, const char *details_json, size_t details_len);

typedef float (*InputFloat)();
typedef int   (*InputInt)();
typedef bool  (*InputBool)();
typedef const char* (*InputString)();
typedef void (*OutputFloat)(float limit);
enum OptionalBool {OptionalTrue, OptionalFalse, OptionalUndefined};
typedef OptionalBool (*PollBool)();
enum TxTrigger_t {TxTrg_Active, TxTrg_Inactive};
enum TxEnableState_t {TxEna_Active, TxEna_Inactive, TxEna_Pending};
typedef TxEnableState_t (*TxStepInOut)(TxTrigger_t triggerIn);


#ifdef __cplusplus
extern "C" {
#endif

/*
 * Please refer to ArduinoOcpp.h for the documentation
 */

void ao_initialize(
            AOcppSocket *osock,  //WebSocket adapter for ArduinoOcpp
            float V_eff, //Grid voltage of your country. e.g. 230.f (European voltage)
            struct AO_FilesystemOpt fsopt); //If this library should format the flash if necessary. Find further options in ConfigurationOptions.h

void ao_deinitialize();

void ao_loop();
/*
 * Send OCPP operations
 */

void ao_bootNotification(const char *chargePointModel, const char *chargePointVendor, OnOcppMessage onConfirmation, OnOcppAbort onAbort, OnOcppTimeout onTimeout, OnOcppError onError);

void ao_bootNotification_full(const char *payloadJson, OnOcppMessage onConfirmation, OnOcppAbort onAbort, OnOcppTimeout onTimeout, OnOcppError onError);

void ao_authorize(const char *idTag, OnOcppMessage onConfirmation, OnOcppAbort onAbort, OnOcppTimeout onTimeout, OnOcppError onError);

/*
 * Charging session management
 */

void ao_beginTransaction(const char *idTag);
void ao_beginTransaction_m(const char *idTag, unsigned int connectorId); //multiple connectors version

bool ao_endTransaction(const char *reason); //reason can be NULL
bool ao_endTransaction_m(const char *reason, unsigned int connectorId); //reason can be NULL

bool ao_isTransactionRunning();
bool ao_isTransactionRunning_m(unsigned int connectorId);

bool ao_ocppPermitsCharge();
bool ao_ocppPermitsCharge_m(unsigned int connectorId);

/*
 * Define the Inputs and Outputs of this library.
 */

void ao_setConnectorPluggedInput(InputBool pluggedInput);
void ao_setConnectorPluggedInput_m(InputBool pluggedInput, unsigned int connectorId);

void ao_setEnergyMeterInput(InputInt energyInput);
void ao_setEnergyMeterInput_m(InputInt energyInput, unsigned int connectorId);

void ao_setPowerMeterInput(InputFloat powerInput);
void ao_setPowerMeterInput_m(InputFloat powerInput, unsigned int connectorId);

void ao_setSmartChargingOutput(OutputFloat chargingLimitOutput);
void ao_setSmartChargingOutput_m(OutputFloat chargingLimitOutput, unsigned int connectorId);

/*
 * Define the Inputs and Outputs of this library. (Advanced)
 */

void ao_setEvReadyInput(InputBool evReadyInput);
void ao_setEvReadyInput_m(InputBool evReadyInput, unsigned int connectorId);

void ao_setEvseReadyInput(InputBool evseReadyInput);
void ao_setEvseReadyInput_m(InputBool evseReadyInput, unsigned int connectorId);

void ao_addErrorCodeInput(InputString errorCodeInput);
void ao_addErrorCodeInput_m(InputString errorCodeInput, unsigned int connectorId);

void ao_addMeterValueInputInt(InputInt valueInput, const char *measurand, const char *unit, const char *location, const char *phase); //measurand, unit, location and phase can be NULL
void ao_addMeterValueInputInt_m(InputInt valueInput, unsigned int connectorId, const char *measurand, const char *unit, const char *location, const char *phase); //measurand, unit, location and phase can be NULL

void ao_addMeterValueInput(MeterValueInput *meterValueInput); //takes ownership of meterValueInput
void ao_addMeterValueInput_m(MeterValueInput *meterValueInput, unsigned int connectorId); //takes ownership of meterValueInput

void ao_setOnUnlockConnectorInOut(PollBool onUnlockConnectorInOut);
void ao_setOnUnlockConnectorInOut_m(PollBool onUnlockConnectorInOut, unsigned int connectorId);

void ao_setConnectorLockInOut(TxStepInOut lockConnectorInOut);
void ao_setConnectorLockInOut_m(TxStepInOut lockConnectorInOut, unsigned int connectorId);

void ao_setTxBasedMeterInOut(TxStepInOut txMeterInOut);
void ao_setTxBasedMeterInOut_m(TxStepInOut txMeterInOut, unsigned int connectorId);

/*
 * Access further information about the internal state of the library
 */

bool ao_isOperative();
bool ao_isOperative_m(unsigned int connectorId);

int ao_getTransactionId();
int ao_getTransactionId_m(unsigned int connectorId);

const char *ao_getTransactionIdTag();
const char *ao_getTransactionIdTag_m(unsigned int connectorId);

void ao_setOnResetRequest(OnOcppMessage onRequest);

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
 * Deprecated functions or functions to be moved to ArduinoOcppExtended.h
 */

void ao_onRemoteStartTransactionSendConf(OnOcppMessage onSendConf); //important, energize the power plug here and capture the idTag

void ao_onRemoteStopTransactionSendConf(OnOcppMessage onSendConf); //important, de-energize the power plug here
void ao_onRemoteStopTransactionRequest(OnOcppMessage onRequest); //optional, to de-energize the power plug immediately

void ao_setOnResetSendConf(OnOcppMessage onSendConf);

void ao_startTransaction(const char *idTag, OnOcppMessage onConfirmation, OnOcppAbort onAbort, OnOcppTimeout onTimeout, OnOcppError onError);

void ao_stopTransaction(OnOcppMessage onConfirmation, OnOcppAbort onAbort, OnOcppTimeout onTimeout, OnOcppError onError);

#ifdef __cplusplus
}
#endif

#endif
