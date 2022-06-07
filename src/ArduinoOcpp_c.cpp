#include "ArduinoOcpp_c.h"
#include "ArduinoOcpp.h"

#include <ArduinoOcpp/Debug.h>

ArduinoOcpp::OcppSocket *ocppSocket = nullptr;

extern "C" void ao_initialize(AOcppSocket *osock) {
    //OCPP_initialize("echo.websocket.events", 80, "ws://echo.websocket.events/");
    if (!osock) {
        AO_DBG_ERR("osock is null");
    }
    AO_DBG_ERR("no error");

    ocppSocket = reinterpret_cast<ArduinoOcpp::OcppSocket*>(osock);

    OCPP_initialize(*ocppSocket);
}

extern "C" void ao_loop() {
    OCPP_loop();
}

extern "C" void ao_set_console_out_c(void (*console_out)(const char *msg)) {
    ao_set_console_out(console_out);
}

#ifndef AO_RECEIVE_PAYLOAD_BUFSIZE
#define AO_RECEIVE_PAYLOAD_BUFSIZE 1024
#endif

char ao_recv_payload_buff [AO_RECEIVE_PAYLOAD_BUFSIZE] = {'\0'};

std::function<void(JsonObject)> adaptCb(OnOcppMessage cb) {
    return [cb] (JsonObject payload) {
        auto len = serializeJson(payload, ao_recv_payload_buff, AO_RECEIVE_PAYLOAD_BUFSIZE);
        if (len <= 0) {
            AO_DBG_WARN("Received payload buffer exceeded. Continue without payload");
        }
        cb(len > 0 ? ao_recv_payload_buff : nullptr, len);
    };
}

std::function<void()> adaptCb(void (*cb)()) {
    return cb;
}

ArduinoOcpp::OnReceiveErrorListener adaptCb(OnOcppError cb) {
    return [cb] (const char *code, const char *description, JsonObject details) {
        auto len = serializeJson(details, ao_recv_payload_buff, AO_RECEIVE_PAYLOAD_BUFSIZE);
        if (len <= 0) {
            AO_DBG_WARN("Received payload buffer exceeded. Continue without payload");
        }
        cb(code, description, len > 0 ? ao_recv_payload_buff : "", len);
    };
}

std::function<bool()> adaptCb(SamplerBool cb) {
    return cb;
}

std::function<const char*()> adaptCb(SamplerString cb) {
    return cb;
}

std::function<float()> adaptCb(SamplerFloat cb) {
    return cb;
}

std::function<int()> adaptCb(SamplerInt cb) {
    return cb;
}

void ao_setPowerActiveImportSampler(SamplerFloat power) {
    setPowerActiveImportSampler(adaptCb(power));
}

void ao_setEnergyActiveImportSampler(SamplerInt energy) {
    setEnergyActiveImportSampler([energy] () -> float {
        return (float) energy();
    });
}

void ao_setEvRequestsEnergySampler(SamplerBool evRequestsEnergy) {
    setEvRequestsEnergySampler(adaptCb(evRequestsEnergy));
}

void ao_setConnectorEnergizedSampler(SamplerBool connectorEnergized) {
    setConnectorEnergizedSampler(adaptCb(connectorEnergized));
}

void ao_setConnectorPluggedSampler(SamplerBool connectorPlugged) {
    setConnectorPluggedSampler(adaptCb(connectorPlugged));
}

void ao_addConnectorErrorCodeSampler(SamplerString connectorErrorCode) {
    addConnectorErrorCodeSampler(adaptCb(connectorErrorCode));
}

void ao_onChargingRateLimitChange(void (*chargingRateChanged)(float)) {
    setOnChargingRateLimitChange(chargingRateChanged);
}

void ao_onUnlockConnector(SamplerBool unlockConnector) {
    setOnUnlockConnector(adaptCb(unlockConnector));
}

void ao_onRemoteStartTransactionSendConf(OnOcppMessage onSendConf) {
    setOnRemoteStopTransactionSendConf(adaptCb(onSendConf));
}

void ao_onRemoteStopTransactionSendConf(OnOcppMessage onSendConf) {
    setOnRemoteStopTransactionSendConf(adaptCb(onSendConf));
}

void ao_onRemoteStopTransactionRequest(OnOcppMessage onRequest) {
    setOnRemoteStopTransactionReceiveReq(adaptCb(onRequest));
}

void ao_onResetSendConf(OnOcppMessage onSendConf) {
    setOnResetSendConf(adaptCb(onSendConf));
}

extern "C" void ao_onResetRequest(OnOcppMessage onRequest) {
    setOnResetReceiveReq(adaptCb(onRequest));
}

extern "C" void ao_bootNotification(const char *chargePointModel, const char *chargePointVendor, OnOcppMessage onConfirmation, OnOcppAbort onAbort, OnOcppTimeout onTimeout, OnOcppError onError) {
    bootNotification("model", "vendor", adaptCb(onConfirmation), adaptCb(onAbort), adaptCb(onTimeout), adaptCb(onError));
}

void ao_authorize(const char *idTag, OnOcppMessage onConfirmation, OnOcppAbort onAbort, OnOcppTimeout onTimeout, OnOcppError onError) {
    authorize(idTag, adaptCb(onConfirmation), adaptCb(onAbort), adaptCb(onTimeout), adaptCb(onError));
}

void ao_startTransaction(const char *idTag, OnOcppMessage onConfirmation, OnOcppAbort onAbort, OnOcppTimeout onTimeout, OnOcppError onError) {
    startTransaction(idTag, adaptCb(onConfirmation), adaptCb(onAbort), adaptCb(onTimeout), adaptCb(onError));
}

void ao_stopTransaction(OnOcppMessage onConfirmation, OnOcppAbort onAbort, OnOcppTimeout onTimeout, OnOcppError onError) {
    stopTransaction(adaptCb(onConfirmation), adaptCb(onAbort), adaptCb(onTimeout), adaptCb(onError));
}

int ao_getTransactionId() {
    return getTransactionId();
}

bool ao_ocppPermitsCharge() {
    return ocppPermitsCharge();
}

bool ao_isAvailable() {
    return isAvailable();
}

void ao_beginSession(const char *idTag) {
    return beginSession(idTag);
}

void ao_endSession() {
    return endSession();
}

bool ao_isInSession() {
    return isInSession();
}

const char *ao_getSessionIdTag() {
    return getSessionIdTag();
}
