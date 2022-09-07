#ifndef ARDUINOOCPP_C_H
#define ARDUINOOCPP_C_H

#include <stddef.h>

struct AOcppSocket;
typedef struct AOcppSocket AOcppSocket;

struct OcppHandle;
typedef struct OcppHandle OcppHandle;

typedef void (*OnOcppMessage) (const char *payload, size_t len);
typedef void (*OnOcppAbort)   ();
typedef void (*OnOcppTimeout) ();
typedef void (*OnOcppError)   (const char *code, const char *description, const char *details_json, size_t details_len);

typedef float (*SamplerFloat)();
typedef int   (*SamplerInt)();
typedef bool  (*SamplerBool)();
typedef const char* (*SamplerString)();

#ifdef __cplusplus
extern "C" {
#endif

void ao_initialize(AOcppSocket *osock);

void ao_deinitialize();

void ao_loop();

void ao_set_console_out_c(void (*console_out)(const char *msg));

/*
 * Feed lib with HW related data
 */

void ao_setPowerActiveImportSampler(SamplerFloat power);

void ao_setEnergyActiveImportSampler(SamplerInt energy);

void ao_addMeterValueSampler_Int(SamplerInt sampler, const char *measurand, const char *phase, const char *unit);

void ao_setEvRequestsEnergySampler(SamplerBool evRequestsEnergy);

void ao_setConnectorEnergizedSampler(SamplerBool connectorEnergized);

void ao_setConnectorPluggedSampler(SamplerBool connectorPlugged);

void ao_addConnectorErrorCodeSampler(SamplerString connectorErrorCode);

/*
 * Execute HW related operations on EVSE
 */

void ao_onChargingRateLimitChange(void (*chargingRateChanged)(float));

void ao_onUnlockConnector(SamplerBool unlockConnector); //true: success, false: failure

/*
 * Generic listeners for OCPP operations initiated by Central System
 */

void ao_onRemoteStartTransactionSendConf(OnOcppMessage onSendConf); //important, energize the power plug here and capture the idTag

void ao_onRemoteStopTransactionSendConf(OnOcppMessage onSendConf); //important, de-energize the power plug here
void ao_onRemoteStopTransactionRequest(OnOcppMessage onRequest); //optional, to de-energize the power plug immediately

void ao_onResetSendConf(OnOcppMessage onSendConf); //important, reset your device here (i.e. call ESP.reset();)
void ao_onResetRequest(OnOcppMessage onRequest); //alternative: start reset timer here

/*
 * Initiate OCPP operations
 */

void ao_bootNotification(const char *chargePointModel, const char *chargePointVendor, OnOcppMessage onConfirmation, OnOcppAbort onAbort, OnOcppTimeout onTimeout, OnOcppError onError);

void ao_bootNotification_full(const char *payloadJson, OnOcppMessage onConfirmation, OnOcppAbort onAbort, OnOcppTimeout onTimeout, OnOcppError onError);

void ao_authorize(const char *idTag, OnOcppMessage onConfirmation, OnOcppAbort onAbort, OnOcppTimeout onTimeout, OnOcppError onError);

void ao_startTransaction(const char *idTag, OnOcppMessage onConfirmation, OnOcppAbort onAbort, OnOcppTimeout onTimeout, OnOcppError onError);

void ao_stopTransaction(OnOcppMessage onConfirmation, OnOcppAbort onAbort, OnOcppTimeout onTimeout, OnOcppError onError);

/*
 * Access OCPP state
 */

int ao_getTransactionId(); //returns the ID of the current transaction. Returns -1 if called before or after an transaction

bool ao_ocppPermitsCharge();

bool ao_isAvailable(); //if the charge point is operative or inoperative

/*
 * Charging session management
 */

void ao_beginSession(const char *idTag);

void ao_endSession();

bool ao_isInSession();

const char *ao_getSessionIdTag();

/*
 * Get access to the internals
 */

OcppHandle *getOcppHandle();

#ifdef __cplusplus
}
#endif

#endif
