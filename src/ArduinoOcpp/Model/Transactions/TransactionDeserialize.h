// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef TRANSACTIONDESERIALIZE_H
#define TRANSACTIONDESERIALIZE_H

#include <ArduinoOcpp/Model/Transactions/Transaction.h>

#include <ArduinoJson.h>

namespace ArduinoOcpp {

const char *serializeStopTxReason(StopTxReason reason);
bool deserializeStopTxReason(const char *reason_cstr, StopTxReason& out);

bool serializeTransaction(Transaction& tx, DynamicJsonDocument& out);
bool deserializeTransaction(Transaction& tx, JsonObject in);

}

#endif
