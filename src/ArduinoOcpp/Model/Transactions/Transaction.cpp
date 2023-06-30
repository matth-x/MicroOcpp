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
