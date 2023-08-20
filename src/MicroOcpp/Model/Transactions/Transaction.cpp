// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>

using namespace MicroOcpp;

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

int ocpp_tx_getTransactionId(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->getTransactionId();
}
bool ocpp_tx_isAuthorized(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->isAuthorized();
}
bool ocpp_tx_isIdTagDeauthorized(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->isIdTagDeauthorized();
}

bool ocpp_tx_isRunning(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->isRunning();
}
bool ocpp_tx_isActive(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->isActive();
}
bool ocpp_tx_isAborted(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->isAborted();
}
bool ocpp_tx_isCompleted(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->isCompleted();
}

const char *ocpp_tx_getIdTag(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->getIdTag();
}

bool ocpp_tx_getBeginTimestamp(OCPP_Transaction *tx, char *buf, size_t len) {
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->getBeginTimestamp().toJsonString(buf, len);
}

int32_t ocpp_tx_getMeterStart(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->getMeterStart();
}

bool ocpp_tx_getStartTimestamp(OCPP_Transaction *tx, char *buf, size_t len) {
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->getStartTimestamp().toJsonString(buf, len);
}

const char *ocpp_tx_getStopIdTag(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->getStopIdTag();
}

int32_t ocpp_tx_getMeterStop(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->getMeterStop();
}

bool ocpp_tx_getStopTimestamp(OCPP_Transaction *tx, char *buf, size_t len) {
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->getStopTimestamp().toJsonString(buf, len);
}

const char *ocpp_tx_getStopReason(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::Transaction*>(tx)->getStopReason();
}
