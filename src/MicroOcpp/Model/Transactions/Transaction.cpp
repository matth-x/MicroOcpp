// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Model/Transactions/Transaction.h>
#include <ArduinoOcpp/Model/Transactions/TransactionStore.h>

using namespace ArduinoOcpp;

bool Transaction::setIdTag(const char *idTag) {
    auto ret = snprintf(this->idTag, IDTAG_LEN_MAX + 1, "%s", idTag);
    return ret >= 0 && ret < IDTAG_LEN_MAX + 1;
}

bool Transaction::setStopIdTag(const char *idTag) {
    auto ret = snprintf(stop_idTag, IDTAG_LEN_MAX + 1, "%s", idTag);
    return ret >= 0 && ret < IDTAG_LEN_MAX + 1;
}

bool Transaction::setStopReason(const char *reason) {
    auto ret = snprintf(stop_reason, REASON_LEN_MAX + 1, "%s", reason);
    return ret >= 0 && ret < REASON_LEN_MAX + 1;
}

bool Transaction::commit() {
    return context.commit(this);
}

int ao_tx_getTransactionId(AO_Transaction *tx) {
    return reinterpret_cast<ArduinoOcpp::Transaction*>(tx)->getTransactionId();
}
bool ao_tx_isAuthorized(AO_Transaction *tx) {
    return reinterpret_cast<ArduinoOcpp::Transaction*>(tx)->isAuthorized();
}
bool ao_tx_isIdTagDeauthorized(AO_Transaction *tx) {
    return reinterpret_cast<ArduinoOcpp::Transaction*>(tx)->isIdTagDeauthorized();
}

bool ao_tx_isRunning(AO_Transaction *tx) {
    return reinterpret_cast<ArduinoOcpp::Transaction*>(tx)->isRunning();
}
bool ao_tx_isActive(AO_Transaction *tx) {
    return reinterpret_cast<ArduinoOcpp::Transaction*>(tx)->isActive();
}
bool ao_tx_isAborted(AO_Transaction *tx) {
    return reinterpret_cast<ArduinoOcpp::Transaction*>(tx)->isAborted();
}
bool ao_tx_isCompleted(AO_Transaction *tx) {
    return reinterpret_cast<ArduinoOcpp::Transaction*>(tx)->isCompleted();
}

const char *ao_tx_getIdTag(AO_Transaction *tx) {
    return reinterpret_cast<ArduinoOcpp::Transaction*>(tx)->getIdTag();
}

bool ao_tx_getBeginTimestamp(AO_Transaction *tx, char *buf, size_t len) {
    return reinterpret_cast<ArduinoOcpp::Transaction*>(tx)->getBeginTimestamp().toJsonString(buf, len);
}

int32_t ao_tx_getMeterStart(AO_Transaction *tx) {
    return reinterpret_cast<ArduinoOcpp::Transaction*>(tx)->getMeterStart();
}

bool ao_tx_getStartTimestamp(AO_Transaction *tx, char *buf, size_t len) {
    return reinterpret_cast<ArduinoOcpp::Transaction*>(tx)->getStartTimestamp().toJsonString(buf, len);
}

const char *ao_tx_getStopIdTag(AO_Transaction *tx) {
    return reinterpret_cast<ArduinoOcpp::Transaction*>(tx)->getStopIdTag();
}

int32_t ao_tx_getMeterStop(AO_Transaction *tx) {
    return reinterpret_cast<ArduinoOcpp::Transaction*>(tx)->getMeterStop();
}

bool ao_tx_getStopTimestamp(AO_Transaction *tx, char *buf, size_t len) {
    return reinterpret_cast<ArduinoOcpp::Transaction*>(tx)->getStopTimestamp().toJsonString(buf, len);
}

const char *ao_tx_getStopReason(AO_Transaction *tx) {
    return reinterpret_cast<ArduinoOcpp::Transaction*>(tx)->getStopReason();
}
