#include "ArduinoOcpp_c.h"
#include "ArduinoOcpp.h"

#include <ArduinoOcpp/Debug.h>

ArduinoOcpp::OcppSocket *ocppSocket = nullptr;

void ao_initialize(AOcppSocket *osock,  float V_eff, struct AO_FilesystemOpt fsopt) {
    if (!osock) {
        AO_DBG_ERR("osock is null");
    }

    ocppSocket = reinterpret_cast<ArduinoOcpp::OcppSocket*>(osock);

    ArduinoOcpp::FilesystemOpt adaptFsopt = fsopt;

    OCPP_initialize(*ocppSocket, V_eff, adaptFsopt);
}

void ao_deinitialize() {
    OCPP_deinitialize();
}

void ao_loop() {
    OCPP_loop();
}

/*
 * Helper functions for transforming callback functions from C-style to C++style
 */

ArduinoOcpp::PollResult<bool> adaptScl(enum OptionalBool v) {
    if (v == OptionalTrue) {
        return true;
    } else if (v == OptionalFalse) {
        return false;
    } else if (v == OptionalNone) {
        return ArduinoOcpp::PollResult<bool>::Await();
    } else {
        AO_DBG_ERR("illegal argument");
        return false;
    }
}

enum TxTrigger_t adaptScl(ArduinoOcpp::TxTrigger v) {
    return v == ArduinoOcpp::TxTrigger::Active ? TxTrigger_t::TxTrg_Active : TxTrigger_t::TxTrg_Inactive;
}

ArduinoOcpp::TxEnableState adaptScl(enum TxEnableState_t v) {
    if (v == TxEna_Pending) {
        return ArduinoOcpp::TxEnableState::Pending;
    } else if (v == TxEna_Active) {
        return ArduinoOcpp::TxEnableState::Active;
    } else if (v == TxEna_Inactive) {
        return ArduinoOcpp::TxEnableState::Inactive;
    } else {
        AO_DBG_ERR("illegal argument");
        return ArduinoOcpp::TxEnableState::Inactive;
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

std::function<void(float)> adaptFn(unsigned int connectorId, OutputFloat_m fn) {
    return [fn, connectorId] (float value) {return fn(connectorId, value);};
}

std::function<void(void)> adaptFn(void (*fn)(void)) {
    return fn;
}

#ifndef AO_RECEIVE_PAYLOAD_BUFSIZE
#define AO_RECEIVE_PAYLOAD_BUFSIZE 512
#endif

char ao_recv_payload_buff [AO_RECEIVE_PAYLOAD_BUFSIZE] = {'\0'};

std::function<void(JsonObject)> adaptFn(OnOcppMessage fn) {
    if (!fn) return nullptr;
    return [fn] (JsonObject payload) {
        auto len = serializeJson(payload, ao_recv_payload_buff, AO_RECEIVE_PAYLOAD_BUFSIZE);
        if (len <= 0) {
            AO_DBG_WARN("Received payload buffer exceeded. Continue without payload");
        }
        fn(len > 0 ? ao_recv_payload_buff : "", len);
    };
}

std::function<void(JsonObject)> adaptFn(const char *idTag_cstr, OnAuthorize fn) {
    if (!fn) return nullptr;
    std::string idTag = idTag_cstr ? idTag_cstr : "undefined";
    return [fn, idTag] (JsonObject payload) {
        auto len = serializeJson(payload, ao_recv_payload_buff, AO_RECEIVE_PAYLOAD_BUFSIZE);
        if (len <= 0) {
            AO_DBG_WARN("Received payload buffer exceeded. Continue without payload");
        }
        fn(idTag.c_str(), len > 0 ? ao_recv_payload_buff : "", len);
    };
}

ArduinoOcpp::OnReceiveErrorListener adaptFn(OnOcppError fn) {
    if (!fn) return nullptr;
    return [fn] (const char *code, const char *description, JsonObject details) {
        auto len = serializeJson(details, ao_recv_payload_buff, AO_RECEIVE_PAYLOAD_BUFSIZE);
        if (len <= 0) {
            AO_DBG_WARN("Received payload buffer exceeded. Continue without payload");
        }
        fn(code, description, len > 0 ? ao_recv_payload_buff : "", len);
    };
}

std::function<ArduinoOcpp::PollResult<bool>()> adaptFn(PollBool fn) {
    return [fn] () {return adaptScl(fn());};
}

std::function<ArduinoOcpp::PollResult<bool>()> adaptFn(unsigned int connectorId, PollBool_m fn) {
    return [fn, connectorId] () {return adaptScl(fn(connectorId));};
}

std::function<ArduinoOcpp::TxEnableState(ArduinoOcpp::TxTrigger)> adaptFn(TxStepInOut fn) {
    if (!fn) return nullptr;
    return [fn] (ArduinoOcpp::TxTrigger trigger) -> ArduinoOcpp::TxEnableState {
        auto res = fn(adaptScl(trigger));
        return adaptScl(res);
    };
}

std::function<ArduinoOcpp::TxEnableState(ArduinoOcpp::TxTrigger)> adaptFn(unsigned int connectorId, TxStepInOut_m fn) {
    if (!fn) return nullptr;
    return [fn, connectorId] (ArduinoOcpp::TxTrigger trigger) -> ArduinoOcpp::TxEnableState {
        auto res = fn(connectorId, adaptScl(trigger));
        return adaptScl(res);
    };
}

void ao_bootNotification(const char *chargePointModel, const char *chargePointVendor, OnOcppMessage onConfirmation, OnOcppAbort onAbort, OnOcppTimeout onTimeout, OnOcppError onError) {
    bootNotification(chargePointModel, chargePointVendor, adaptFn(onConfirmation), adaptFn(onAbort), adaptFn(onTimeout), adaptFn(onError));
}

void ao_bootNotification_full(const char *payloadJson, OnOcppMessage onConfirmation, OnOcppAbort onAbort, OnOcppTimeout onTimeout, OnOcppError onError) {
    auto payload = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(9) + 230 + 9)); // BootNotification has at most 9 attributes with at most 230 chars + null terminators
    auto err = deserializeJson(*payload, payloadJson);
    if (err) {
        AO_DBG_ERR("Could not process input: %s", err.c_str());
        (void)0;
    }

    bootNotification(std::move(payload), adaptFn(onConfirmation), adaptFn(onAbort), adaptFn(onTimeout), adaptFn(onError));
}

void ao_authorize(const char *idTag, OnAuthorize onConfirmation, OnOcppAbort onAbort, OnOcppTimeout onTimeout, OnOcppError onError) {
    authorize(idTag, adaptFn(idTag, onConfirmation), adaptFn(onAbort), adaptFn(onTimeout), adaptFn(onError));
}

void ao_beginTransaction(const char *idTag) {
    ao_beginTransaction(idTag);
}
void ao_beginTransaction_m(unsigned int connectorId, const char *idTag) {
    beginTransaction(idTag, 1);
}

bool ao_endTransaction(const char *reason) {
    return endTransaction(reason);
}
bool ao_endTransaction_m(unsigned int connectorId, const char *reason) {
    return endTransaction(reason, connectorId);
}

bool ao_isTransactionRunning() {
    return isTransactionRunning();
}
bool ao_isTransactionRunning_m(unsigned int connectorId) {
    return isTransactionRunning(connectorId);
}

bool ao_ocppPermitsCharge() {
    return ocppPermitsCharge();
}
bool ao_ocppPermitsCharge_m(unsigned int connectorId) {
    return ocppPermitsCharge(connectorId);
}

void ao_setConnectorPluggedInput(InputBool pluggedInput) {
    setConnectorPluggedInput(adaptFn(pluggedInput));
}
void ao_setConnectorPluggedInput_m(unsigned int connectorId, InputBool_m pluggedInput) {
    setConnectorPluggedInput(adaptFn(connectorId, pluggedInput), connectorId);
}

void ao_setEnergyMeterInput(InputInt energyInput) {
    setEnergyMeterInput(adaptFn(energyInput));
}
void ao_setEnergyMeterInput_m(unsigned int connectorId, InputInt_m energyInput) {
    setEnergyMeterInput(adaptFn(connectorId, energyInput), connectorId);
}

void ao_setPowerMeterInput(InputFloat powerInput) {
    setPowerMeterInput(adaptFn(powerInput));
}
void ao_setPowerMeterInput_m(unsigned int connectorId, InputFloat_m powerInput) {
    setPowerMeterInput(adaptFn(connectorId, powerInput), connectorId);
}

void ao_setSmartChargingOutput(OutputFloat chargingLimitOutput) {
    setSmartChargingOutput(adaptFn(chargingLimitOutput));
}
void ao_setSmartChargingOutput_m(unsigned int connectorId, OutputFloat_m chargingLimitOutput) {
    setSmartChargingOutput(adaptFn(connectorId, chargingLimitOutput), connectorId);
}

void ao_setEvReadyInput(InputBool evReadyInput) {
    setEvReadyInput(adaptFn(evReadyInput));
}
void ao_setEvReadyInput_m(unsigned int connectorId, InputBool_m evReadyInput) {
    setEvReadyInput(adaptFn(connectorId, evReadyInput), connectorId);
}

void ao_setEvseReadyInput(InputBool evseReadyInput) {
    setEvseReadyInput(adaptFn(evseReadyInput));
}
void ao_setEvseReadyInput_m(unsigned int connectorId, InputBool_m evseReadyInput) {
    setEvseReadyInput(adaptFn(connectorId, evseReadyInput), connectorId);
}

void ao_addErrorCodeInput(InputString errorCodeInput) {
    addErrorCodeInput(adaptFn(errorCodeInput));
}
void ao_addErrorCodeInput_m(unsigned int connectorId, InputString_m errorCodeInput) {
    addErrorCodeInput(adaptFn(connectorId, errorCodeInput), connectorId);
}

void ao_addMeterValueInputInt(InputInt valueInput, const char *measurand, const char *unit, const char *location, const char *phase) {
    addMeterValueInput(adaptFn(valueInput), 1, measurand, unit, location, phase);
}
void ao_addMeterValueInputInt_m(unsigned int connectorId, InputInt_m valueInput, const char *measurand, const char *unit, const char *location, const char *phase) {
    addMeterValueInput(adaptFn(connectorId, valueInput), connectorId, measurand, unit, location, phase);
}

void ao_addMeterValueInput(MeterValueInput *meterValueInput) {
    ao_addMeterValueInput_m(1, meterValueInput);
}
void ao_addMeterValueInput_m(unsigned int connectorId, MeterValueInput *meterValueInput) {
    auto svs = std::unique_ptr<ArduinoOcpp::SampledValueSampler>(
        reinterpret_cast<ArduinoOcpp::SampledValueSampler*>(meterValueInput));
    
    addMeterValueInput(std::move(svs), connectorId);
}

void ao_setOnUnlockConnectorInOut(PollBool onUnlockConnectorInOut) {
    setOnUnlockConnectorInOut(adaptFn(onUnlockConnectorInOut));
}
void ao_setOnUnlockConnectorInOut_m(unsigned int connectorId, PollBool_m onUnlockConnectorInOut) {
    setOnUnlockConnectorInOut(adaptFn(connectorId, onUnlockConnectorInOut), connectorId);
}

void ao_setConnectorLockInOut(TxStepInOut lockConnectorInOut) {
    setConnectorLockInOut(adaptFn(lockConnectorInOut));
}
void ao_setConnectorLockInOut_m(unsigned int connectorId, TxStepInOut_m lockConnectorInOut) {
    setConnectorLockInOut(adaptFn(connectorId, lockConnectorInOut), connectorId);
}

void ao_setTxBasedMeterInOut(TxStepInOut txMeterInOut) {
    setTxBasedMeterInOut(adaptFn(txMeterInOut));
}
void ao_setTxBasedMeterInOut_m(unsigned int connectorId, TxStepInOut_m txMeterInOut) {
    setTxBasedMeterInOut(adaptFn(connectorId, txMeterInOut), connectorId);
}

bool ao_isOperative() {
    return isOperative();
}
bool ao_isOperative_m(unsigned int connectorId) {
    return isOperative(connectorId);
}

int ao_getTransactionId() {
    return getTransactionId();
}
int ao_getTransactionId_m(unsigned int connectorId) {
    return getTransactionId(connectorId);
}

const char *ao_getTransactionIdTag() {
    return getTransactionIdTag();
}
const char *ao_getTransactionIdTag_m(unsigned int connectorId) {
    return getTransactionIdTag(connectorId);
}

void ao_setOnResetRequest(OnOcppMessage onRequest) {
    setOnResetRequest(adaptFn(onRequest));
}

void ao_set_console_out_c(void (*console_out)(const char *msg)) {
    ao_set_console_out(console_out);
}

OcppHandle *getOcppHandle() {
    return reinterpret_cast<OcppHandle*>(getOcppEngine());
}

void ao_onRemoteStartTransactionSendConf(OnOcppMessage onSendConf) {
    setOnRemoteStopTransactionSendConf(adaptFn(onSendConf));
}

void ao_onRemoteStopTransactionSendConf(OnOcppMessage onSendConf) {
    setOnRemoteStopTransactionSendConf(adaptFn(onSendConf));
}

void ao_onRemoteStopTransactionRequest(OnOcppMessage onRequest) {
    setOnRemoteStopTransactionReceiveReq(adaptFn(onRequest));
}

void ao_onResetSendConf(OnOcppMessage onSendConf) {
    setOnResetSendConf(adaptFn(onSendConf));
}

void ao_startTransaction(const char *idTag, OnOcppMessage onConfirmation, OnOcppAbort onAbort, OnOcppTimeout onTimeout, OnOcppError onError) {
    startTransaction(idTag, adaptFn(onConfirmation), adaptFn(onAbort), adaptFn(onTimeout), adaptFn(onError));
}

void ao_stopTransaction(OnOcppMessage onConfirmation, OnOcppAbort onAbort, OnOcppTimeout onTimeout, OnOcppError onError) {
    stopTransaction(adaptFn(onConfirmation), adaptFn(onAbort), adaptFn(onTimeout), adaptFn(onError));
}
