// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_MICROOCPP_C_H
#define MO_MICROOCPP_C_H

#include <stddef.h>

#include <MicroOcpp/Core/ConfigurationOptions.h>
#include <MicroOcpp/Model/ConnectorBase/ChargePointStatus.h>
#include <MicroOcpp/Model/ConnectorBase/UnlockConnectorResult.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Model/Certificates/Certificate_c.h>
#include <MicroOcpp/Model/Metering/ReadingContext.h>

struct OCPP_Connection;
typedef struct OCPP_Connection OCPP_Connection;

struct MeterValueInput;
typedef struct MeterValueInput MeterValueInput;

struct FilesystemAdapterC;
typedef struct FilesystemAdapterC FilesystemAdapterC;

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

#if MO_ENABLE_CONNECTOR_LOCK
typedef UnlockConnectorResult (*PollUnlockResult)();
typedef UnlockConnectorResult (*PollUnlockResult_m)(unsigned int connectorId);
#endif //MO_ENABLE_CONNECTOR_LOCK


#ifdef __cplusplus
extern "C" {
#endif

/*
 * Please refer to MicroOcpp.h for the documentation
 */

void ocpp_initialize(
            OCPP_Connection *conn,  //WebSocket adapter for MicroOcpp
            const char *chargePointModel,     //model name of this charger (e.g. "My Charger")
            const char *chargePointVendor, //brand name (e.g. "My Company Ltd.")
            struct OCPP_FilesystemOpt fsopt, //If this library should format the flash if necessary. Find further options in ConfigurationOptions.h
            bool autoRecover, //automatically sanitize the local data store when the lib detects recurring crashes. During development, `false` is recommended
            bool ocpp201); //true to select OCPP 2.0.1, false for OCPP 1.6

//same as above, but more fields for the BootNotification
void ocpp_initialize_full(
            OCPP_Connection *conn,  //WebSocket adapter for MicroOcpp
            const char *bootNotificationCredentials, //e.g. '{"chargePointModel":"Demo Charger","chargePointVendor":"My Company Ltd."}' (refer to OCPP 1.6 Specification - Edition 2 p. 60)
            struct OCPP_FilesystemOpt fsopt, //If this library should format the flash if necessary. Find further options in ConfigurationOptions.h
            bool autoRecover, //automatically sanitize the local data store when the lib detects recurring crashes. During development, `false` is recommended
            bool ocpp201); //true to select OCPP 2.0.1, false for OCPP 1.6

//same as above, but pass FS handle instead of FS options
void ocpp_initialize_full2(
            OCPP_Connection *conn,  //WebSocket adapter for MicroOcpp
            const char *bootNotificationCredentials, //e.g. '{"chargePointModel":"Demo Charger","chargePointVendor":"My Company Ltd."}' (refer to OCPP 1.6 Specification - Edition 2 p. 60)
            FilesystemAdapterC *filesystem, //FilesystemAdapter handle initialized by client. MO takes ownership and deletes it during deinitialization
            bool autoRecover, //automatically sanitize the local data store when the lib detects recurring crashes. During development, `false` is recommended
            bool ocpp201); //true to select OCPP 2.0.1, false for OCPP 1.6

void ocpp_deinitialize();

bool ocpp_is_initialized();

void ocpp_loop();

/*
 * Charging session management
 */

bool ocpp_beginTransaction(const char *idTag);
bool ocpp_beginTransaction_m(unsigned int connectorId, const char *idTag); //multiple connectors version

bool ocpp_beginTransaction_authorized(const char *idTag, const char *parentIdTag);
bool ocpp_beginTransaction_authorized_m(unsigned int connectorId, const char *idTag, const char *parentIdTag);

bool ocpp_endTransaction(const char *idTag, const char *reason); //idTag, reason can be NULL
bool ocpp_endTransaction_m(unsigned int connectorId, const char *idTag, const char *reason); //idTag, reason can be NULL

bool ocpp_endTransaction_authorized(const char *idTag, const char *reason); //idTag, reason can be NULL
bool ocpp_endTransaction_authorized_m(unsigned int connectorId, const char *idTag, const char *reason); //idTag, reason can be NULL

bool ocpp_isTransactionActive();
bool ocpp_isTransactionActive_m(unsigned int connectorId);

bool ocpp_isTransactionRunning();
bool ocpp_isTransactionRunning_m(unsigned int connectorId);

const char *ocpp_getTransactionIdTag();
const char *ocpp_getTransactionIdTag_m(unsigned int connectorId);

OCPP_Transaction *ocpp_getTransaction();
OCPP_Transaction *ocpp_getTransaction_m(unsigned int connectorId);

bool ocpp_ocppPermitsCharge();
bool ocpp_ocppPermitsCharge_m(unsigned int connectorId);

ChargePointStatus ocpp_getChargePointStatus();
ChargePointStatus ocpp_getChargePointStatus_m(unsigned int connectorId);

/*
 * Define the Inputs and Outputs of this library.
 */

void ocpp_setConnectorPluggedInput(InputBool pluggedInput);
void ocpp_setConnectorPluggedInput_m(unsigned int connectorId, InputBool_m pluggedInput);

void ocpp_setEnergyMeterInput(InputInt energyInput);
void ocpp_setEnergyMeterInput_m(unsigned int connectorId, InputInt_m energyInput);

void ocpp_setPowerMeterInput(InputFloat powerInput);
void ocpp_setPowerMeterInput_m(unsigned int connectorId, InputFloat_m powerInput);

void ocpp_setSmartChargingPowerOutput(OutputFloat maxPowerOutput);
void ocpp_setSmartChargingPowerOutput_m(unsigned int connectorId, OutputFloat_m maxPowerOutput);
void ocpp_setSmartChargingCurrentOutput(OutputFloat maxCurrentOutput);
void ocpp_setSmartChargingCurrentOutput_m(unsigned int connectorId, OutputFloat_m maxCurrentOutput);
void ocpp_setSmartChargingOutput(OutputSmartCharging chargingLimitOutput);
void ocpp_setSmartChargingOutput_m(unsigned int connectorId, OutputSmartCharging_m chargingLimitOutput);

/*
 * Define the Inputs and Outputs of this library. (Advanced)
 */

void ocpp_setEvReadyInput(InputBool evReadyInput);
void ocpp_setEvReadyInput_m(unsigned int connectorId, InputBool_m evReadyInput);

void ocpp_setEvseReadyInput(InputBool evseReadyInput);
void ocpp_setEvseReadyInput_m(unsigned int connectorId, InputBool_m evseReadyInput);

void ocpp_addErrorCodeInput(InputString errorCodeInput);
void ocpp_addErrorCodeInput_m(unsigned int connectorId, InputString_m errorCodeInput);

void ocpp_addMeterValueInputFloat(InputFloat valueInput, const char *measurand, const char *unit, const char *location, const char *phase); //measurand, unit, location and phase can be NULL
void ocpp_addMeterValueInputFloat_m(unsigned int connectorId, InputFloat_m valueInput, const char *measurand, const char *unit, const char *location, const char *phase); //measurand, unit, location and phase can be NULL

void ocpp_addMeterValueInputIntTx(int (*valueInput)(ReadingContext), const char *measurand, const char *unit, const char *location, const char *phase); //measurand, unit, location and phase can be NULL
void ocpp_addMeterValueInputIntTx_m(unsigned int connectorId, int (*valueInput)(unsigned int cId, ReadingContext), const char *measurand, const char *unit, const char *location, const char *phase); //measurand, unit, location and phase can be NULL

void ocpp_addMeterValueInput(MeterValueInput *meterValueInput); //takes ownership of meterValueInput
void ocpp_addMeterValueInput_m(unsigned int connectorId, MeterValueInput *meterValueInput); //takes ownership of meterValueInput

void ocpp_setOccupiedInput(InputBool occupied);
void ocpp_setOccupiedInput_m(unsigned int connectorId, InputBool_m occupied);

void ocpp_setStartTxReadyInput(InputBool startTxReady);
void ocpp_setStartTxReadyInput_m(unsigned int connectorId, InputBool_m startTxReady);

void ocpp_setStopTxReadyInput(InputBool stopTxReady);
void ocpp_setStopTxReadyInput_m(unsigned int connectorId, InputBool_m stopTxReady);

void ocpp_setTxNotificationOutput(void (*notificationOutput)(OCPP_Transaction*, TxNotification));
void ocpp_setTxNotificationOutput_m(unsigned int connectorId, void (*notificationOutput)(unsigned int, OCPP_Transaction*, TxNotification));

#if MO_ENABLE_CONNECTOR_LOCK
void ocpp_setOnUnlockConnectorInOut(PollUnlockResult onUnlockConnectorInOut);
void ocpp_setOnUnlockConnectorInOut_m(unsigned int connectorId, PollUnlockResult_m onUnlockConnectorInOut);
#endif //MO_ENABLE_CONNECTOR_LOCK

/*
 * Access further information about the internal state of the library
 */

bool ocpp_isOperative();
bool ocpp_isOperative_m(unsigned int connectorId);

void ocpp_setOnResetNotify(bool (*onResetNotify)(bool));

void ocpp_setOnResetExecute(void (*onResetExecute)(bool));

#if MO_ENABLE_CERT_MGMT
void ocpp_setCertificateStore(ocpp_cert_store *certs);
#endif //MO_ENABLE_CERT_MGMT

void ocpp_setOnReceiveRequest(const char *operationType, OnMessage onRequest);

void ocpp_setOnSendConf(const char *operationType, OnMessage onConfirmation);

/*
 * Send OCPP operations
 */
void ocpp_authorize(const char *idTag, AuthorizeConfCallback onConfirmation, AuthorizeAbortCallback onAbort, AuthorizeTimeoutCallback onTimeout, AuthorizeErrorCallback onError, void *user_data);

void ocpp_startTransaction(const char *idTag, OnMessage onConfirmation, OnAbort onAbort, OnTimeout onTimeout, OnCallError onError);

void ocpp_stopTransaction(OnMessage onConfirmation, OnAbort onAbort, OnTimeout onTimeout, OnCallError onError);

#ifdef __cplusplus
}
#endif

#endif
