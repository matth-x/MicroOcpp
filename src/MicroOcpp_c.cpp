// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include "MicroOcpp_c.h"
#include "MicroOcpp.h"

#include <MicroOcpp/Debug.h>

MicroOcpp::Connection *ocppSocket = nullptr;

void ocpp_initialize(OCPP_Connection *conn, const char *chargePointModel, const char *chargePointVendor, struct OCPP_FilesystemOpt fsopt, bool autoRecover) {
    ocpp_initialize_full(conn, ChargerCredentials(chargePointModel, chargePointVendor), fsopt, autoRecover);
}

void ocpp_initialize_full(OCPP_Connection *conn, const char *bootNotificationCredentials, struct OCPP_FilesystemOpt fsopt, bool autoRecover) {
    if (!conn) {
        MO_DBG_ERR("conn is null");
    }

    ocppSocket = reinterpret_cast<MicroOcpp::Connection*>(conn);

    MicroOcpp::FilesystemOpt adaptFsopt = fsopt;

    mocpp_initialize(*ocppSocket, bootNotificationCredentials, MicroOcpp::makeDefaultFilesystemAdapter(adaptFsopt), autoRecover);
}

void ocpp_deinitialize() {
    mocpp_deinitialize();
}

void ocpp_loop() {
    mocpp_loop();
}

/*
 * Helper functions for transforming callback functions from C-style to C++style
 */

MicroOcpp::PollResult<bool> adaptScl(enum OptionalBool v) {
    if (v == OptionalTrue) {
        return true;
    } else if (v == OptionalFalse) {
        return false;
    } else if (v == OptionalNone) {
        return MicroOcpp::PollResult<bool>::Await();
    } else {
        MO_DBG_ERR("illegal argument");
        return false;
    }
}

std::function<bool()> adaptFn(InputBool fn) {
    return fn;
}

std::function<bool()> adaptFn(unsigned int connectorId, InputBool_m fn) {
    return [fn, connectorId] () {return fn(connectorId);};
}

std::function<const char*()> adaptFn(InputString fn) {
    return fn;
}

std::function<const char*()> adaptFn(unsigned int connectorId, InputString_m fn) {
    return [fn, connectorId] () {return fn(connectorId);};
}

std::function<float()> adaptFn(InputFloat fn) {
    return fn;
}

std::function<float()> adaptFn(unsigned int connectorId, InputFloat_m fn) {
    return [fn, connectorId] () {return fn(connectorId);};
}

std::function<int()> adaptFn(InputInt fn) {
    return fn;
}

std::function<int()> adaptFn(unsigned int connectorId, InputInt_m fn) {
    return [fn, connectorId] () {return fn(connectorId);};
}

std::function<void(float)> adaptFn(OutputFloat fn) {
    return fn;
}

std::function<void(float, float, int)> adaptFn(OutputSmartCharging fn) {
    return fn;
}

std::function<void(float, float, int)> adaptFn(unsigned int connectorId, OutputSmartCharging_m fn) {
    return [fn, connectorId] (float power, float current, int nphases) {fn(connectorId, power, current, nphases);};
}

std::function<void(float)> adaptFn(unsigned int connectorId, OutputFloat_m fn) {
    return [fn, connectorId] (float value) {return fn(connectorId, value);};
}

std::function<void(void)> adaptFn(void (*fn)(void)) {
    return fn;
}

#ifndef MO_RECEIVE_PAYLOAD_BUFSIZE
#define MO_RECEIVE_PAYLOAD_BUFSIZE 512
#endif

char ocpp_recv_payload_buff [MO_RECEIVE_PAYLOAD_BUFSIZE] = {'\0'};

std::function<void(JsonObject)> adaptFn(OnMessage fn) {
    if (!fn) return nullptr;
    return [fn] (JsonObject payload) {
        auto len = serializeJson(payload, ocpp_recv_payload_buff, MO_RECEIVE_PAYLOAD_BUFSIZE);
        if (len <= 0) {
            MO_DBG_WARN("Received payload buffer exceeded. Continue without payload");
        }
        fn(len > 0 ? ocpp_recv_payload_buff : "", len);
    };
}

MicroOcpp::OnReceiveErrorListener adaptFn(OnCallError fn) {
    if (!fn) return nullptr;
    return [fn] (const char *code, const char *description, JsonObject details) {
        auto len = serializeJson(details, ocpp_recv_payload_buff, MO_RECEIVE_PAYLOAD_BUFSIZE);
        if (len <= 0) {
            MO_DBG_WARN("Received payload buffer exceeded. Continue without payload");
        }
        fn(code, description, len > 0 ? ocpp_recv_payload_buff : "", len);
    };
}

std::function<MicroOcpp::PollResult<bool>()> adaptFn(PollBool fn) {
    return [fn] () {return adaptScl(fn());};
}

std::function<MicroOcpp::PollResult<bool>()> adaptFn(unsigned int connectorId, PollBool_m fn) {
    return [fn, connectorId] () {return adaptScl(fn(connectorId));};
}

void ocpp_beginTransaction(const char *idTag) {
    beginTransaction(idTag);
}
void ocpp_beginTransaction_m(unsigned int connectorId, const char *idTag) {
    beginTransaction(idTag, connectorId);
}

void ocpp_beginTransaction_authorized(const char *idTag, const char *parentIdTag) {
    beginTransaction_authorized(idTag, parentIdTag);
}
void ocpp_beginTransaction_authorized_m(unsigned int connectorId, const char *idTag, const char *parentIdTag) {
    beginTransaction_authorized(idTag, parentIdTag, connectorId);
}

bool ocpp_endTransaction(const char *idTag, const char *reason) {
    return endTransaction(idTag, reason);
}
bool ocpp_endTransaction_m(unsigned int connectorId, const char *idTag, const char *reason) {
    return endTransaction(idTag, reason, connectorId);
}

bool ocpp_endTransaction_authorized(const char *idTag, const char *reason) {
    return endTransaction_authorized(idTag, reason);
}
bool ocpp_endTransaction_authorized_m(unsigned int connectorId, const char *idTag, const char *reason) {
    return endTransaction_authorized(idTag, reason, connectorId);
}

bool ocpp_isTransactionActive() {
    return isTransactionActive();
}
bool ocpp_isTransactionActive_m(unsigned int connectorId) {
    return isTransactionActive(connectorId);
}

bool ocpp_isTransactionRunning() {
    return isTransactionRunning();
}
bool ocpp_isTransactionRunning_m(unsigned int connectorId) {
    return isTransactionRunning(connectorId);
}

const char *ocpp_getTransactionIdTag() {
    return getTransactionIdTag();
}
const char *ocpp_getTransactionIdTag_m(unsigned int connectorId) {
    return getTransactionIdTag(connectorId);
}

OCPP_Transaction *ocpp_getTransaction() {
    return ocpp_getTransaction_m(1);
}
OCPP_Transaction *ocpp_getTransaction_m(unsigned int connectorId) {
    if (getTransaction(connectorId)) {
        return reinterpret_cast<OCPP_Transaction*>(getTransaction(connectorId).get());
    } else {
        return NULL;
    }
}

bool ocpp_ocppPermitsCharge() {
    return ocppPermitsCharge();
}
bool ocpp_ocppPermitsCharge_m(unsigned int connectorId) {
    return ocppPermitsCharge(connectorId);
}

void ocpp_setConnectorPluggedInput(InputBool pluggedInput) {
    setConnectorPluggedInput(adaptFn(pluggedInput));
}
void ocpp_setConnectorPluggedInput_m(unsigned int connectorId, InputBool_m pluggedInput) {
    setConnectorPluggedInput(adaptFn(connectorId, pluggedInput), connectorId);
}

void ocpp_setEnergyMeterInput(InputInt energyInput) {
    setEnergyMeterInput(adaptFn(energyInput));
}
void ocpp_setEnergyMeterInput_m(unsigned int connectorId, InputInt_m energyInput) {
    setEnergyMeterInput(adaptFn(connectorId, energyInput), connectorId);
}

void ocpp_setPowerMeterInput(InputFloat powerInput) {
    setPowerMeterInput(adaptFn(powerInput));
}
void ocpp_setPowerMeterInput_m(unsigned int connectorId, InputFloat_m powerInput) {
    setPowerMeterInput(adaptFn(connectorId, powerInput), connectorId);
}

void ocpp_setSmartChargingPowerOutput(OutputFloat maxPowerOutput) {
    setSmartChargingPowerOutput(adaptFn(maxPowerOutput));
}
void ocpp_setSmartChargingPowerOutput_m(unsigned int connectorId, OutputFloat_m maxPowerOutput) {
    setSmartChargingPowerOutput(adaptFn(connectorId, maxPowerOutput), connectorId);
}
void ocpp_setSmartChargingCurrentOutput(OutputFloat maxCurrentOutput) {
    setSmartChargingCurrentOutput(adaptFn(maxCurrentOutput));
}
void ocpp_setSmartChargingCurrentOutput_m(unsigned int connectorId, OutputFloat_m maxCurrentOutput) {
    setSmartChargingCurrentOutput(adaptFn(connectorId, maxCurrentOutput), connectorId);
}
void ocpp_setSmartChargingOutput(OutputSmartCharging chargingLimitOutput) {
    setSmartChargingOutput(adaptFn(chargingLimitOutput));
}
void ocpp_setSmartChargingOutput_m(unsigned int connectorId, OutputSmartCharging_m chargingLimitOutput) {
    setSmartChargingOutput(adaptFn(connectorId, chargingLimitOutput), connectorId);
}

void ocpp_setEvReadyInput(InputBool evReadyInput) {
    setEvReadyInput(adaptFn(evReadyInput));
}
void ocpp_setEvReadyInput_m(unsigned int connectorId, InputBool_m evReadyInput) {
    setEvReadyInput(adaptFn(connectorId, evReadyInput), connectorId);
}

void ocpp_setEvseReadyInput(InputBool evseReadyInput) {
    setEvseReadyInput(adaptFn(evseReadyInput));
}
void ocpp_setEvseReadyInput_m(unsigned int connectorId, InputBool_m evseReadyInput) {
    setEvseReadyInput(adaptFn(connectorId, evseReadyInput), connectorId);
}

void ocpp_addErrorCodeInput(InputString errorCodeInput) {
    addErrorCodeInput(adaptFn(errorCodeInput));
}
void ocpp_addErrorCodeInput_m(unsigned int connectorId, InputString_m errorCodeInput) {
    addErrorCodeInput(adaptFn(connectorId, errorCodeInput), connectorId);
}

void ocpp_addMeterValueInputFloat(InputFloat valueInput, const char *measurand, const char *unit, const char *location, const char *phase) {
    addMeterValueInput(adaptFn(valueInput), measurand, unit, location, phase, 1);
}
void ocpp_addMeterValueInputFloat_m(unsigned int connectorId, InputFloat_m valueInput, const char *measurand, const char *unit, const char *location, const char *phase) {
    addMeterValueInput(adaptFn(connectorId, valueInput), measurand, unit, location, phase, connectorId);
}

void ocpp_addMeterValueInput(MeterValueInput *meterValueInput) {
    ocpp_addMeterValueInput_m(1, meterValueInput);
}
void ocpp_addMeterValueInput_m(unsigned int connectorId, MeterValueInput *meterValueInput) {
    auto svs = std::unique_ptr<MicroOcpp::SampledValueSampler>(
        reinterpret_cast<MicroOcpp::SampledValueSampler*>(meterValueInput));
    
    addMeterValueInput(std::move(svs), connectorId);
}

void ocpp_setOnUnlockConnectorInOut(PollBool onUnlockConnectorInOut) {
    setOnUnlockConnectorInOut(adaptFn(onUnlockConnectorInOut));
}
void ocpp_setOnUnlockConnectorInOut_m(unsigned int connectorId, PollBool_m onUnlockConnectorInOut) {
    setOnUnlockConnectorInOut(adaptFn(connectorId, onUnlockConnectorInOut), connectorId);
}

void ocpp_setStartTxReadyInput(InputBool startTxReady) {
    setStartTxReadyInput(adaptFn(startTxReady));
}
void ocpp_setStartTxReadyInput_m(unsigned int connectorId, InputBool_m startTxReady) {
    setStartTxReadyInput(adaptFn(connectorId, startTxReady), connectorId);
}

void ocpp_setStopTxReadyInput(InputBool stopTxReady) {
    setStopTxReadyInput(adaptFn(stopTxReady));
}
void ocpp_setStopTxReadyInput_m(unsigned int connectorId, InputBool_m stopTxReady) {
    setStopTxReadyInput(adaptFn(connectorId, stopTxReady), connectorId);
}

void ocpp_setTxNotificationOutput(void (*notificationOutput)(OCPP_Transaction*, enum OCPP_TxNotification)) {
    setTxNotificationOutput([notificationOutput] (MicroOcpp::Transaction *tx, MicroOcpp::TxNotification notification) {
        notificationOutput(reinterpret_cast<OCPP_Transaction*>(tx), convertTxNotification(notification));
    });
}
void ocpp_setTxNotificationOutput_m(unsigned int connectorId, void (*notificationOutput)(unsigned int, OCPP_Transaction*, enum OCPP_TxNotification)) {
    setTxNotificationOutput([notificationOutput, connectorId] (MicroOcpp::Transaction *tx, MicroOcpp::TxNotification notification) {
        notificationOutput(connectorId, reinterpret_cast<OCPP_Transaction*>(tx), convertTxNotification(notification));
    }, connectorId);
}

void ocpp_setOccupiedInput(InputBool occupied) {
    setOccupiedInput(adaptFn(occupied));
}
void ocpp_setOccupiedInput_m(unsigned int connectorId, InputBool_m occupied) {
    setOccupiedInput(adaptFn(connectorId, occupied), connectorId);
}

bool ocpp_isOperative() {
    return isOperative();
}
bool ocpp_isOperative_m(unsigned int connectorId) {
    return isOperative(connectorId);
}
void ocpp_setOnResetNotify(bool (*onResetNotify)(bool)) {
    setOnResetNotify([onResetNotify] (bool isHard) {return onResetNotify(isHard);});
}

void ocpp_setOnResetExecute(void (*onResetExecute)(bool)) {
    setOnResetExecute([onResetExecute] (bool isHard) {onResetExecute(isHard);});
}

void ocpp_setOnReceiveRequest(const char *operationType, OnMessage onRequest) {
    setOnReceiveRequest(operationType, adaptFn(onRequest));
}

void ocpp_setOnSendConf(const char *operationType, OnMessage onConfirmation) {
    setOnSendConf(operationType, adaptFn(onConfirmation));
}

void ocpp_set_console_out_c(void (*console_out)(const char *msg)) {
    mocpp_set_console_out(console_out);
}

void ocpp_authorize(const char *idTag, AuthorizeConfCallback onConfirmation, AuthorizeAbortCallback onAbort, AuthorizeTimeoutCallback onTimeout, AuthorizeErrorCallback onError, void *user_data) {
    
    std::string idTag_capture = idTag;

    authorize(idTag,
            onConfirmation ? [onConfirmation, idTag_capture, user_data] (JsonObject payload) {
                    auto len = serializeJson(payload, ocpp_recv_payload_buff, MO_RECEIVE_PAYLOAD_BUFSIZE);
                    if (len <= 0) {MO_DBG_WARN("Received payload buffer exceeded. Continue without payload");}
                    onConfirmation(idTag_capture.c_str(), len > 0 ? ocpp_recv_payload_buff : "", len, user_data);
                } : OnReceiveConfListener(nullptr),
            onAbort ? [onAbort, idTag_capture, user_data] () -> void {
                    onAbort(idTag_capture.c_str(), user_data);
                } : OnAbortListener(nullptr),
            onTimeout ? [onTimeout, idTag_capture, user_data] () {
                    onTimeout(idTag_capture.c_str(), user_data);
                } : OnTimeoutListener(nullptr),
            onError ? [onError, idTag_capture, user_data] (const char *code, const char *description, JsonObject details) {
                    auto len = serializeJson(details, ocpp_recv_payload_buff, MO_RECEIVE_PAYLOAD_BUFSIZE);
                    if (len <= 0) {MO_DBG_WARN("Received payload buffer exceeded. Continue without payload");}
                    onError(idTag_capture.c_str(), code, description, len > 0 ? ocpp_recv_payload_buff : "", len, user_data);
                } : OnReceiveErrorListener(nullptr));
}

void ocpp_startTransaction(const char *idTag, OnMessage onConfirmation, OnAbort onAbort, OnTimeout onTimeout, OnCallError onError) {
    startTransaction(idTag, adaptFn(onConfirmation), adaptFn(onAbort), adaptFn(onTimeout), adaptFn(onError));
}

void ocpp_stopTransaction(OnMessage onConfirmation, OnAbort onAbort, OnTimeout onTimeout, OnCallError onError) {
    stopTransaction(adaptFn(onConfirmation), adaptFn(onAbort), adaptFn(onTimeout), adaptFn(onError));
}
