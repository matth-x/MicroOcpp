// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef TRANSACTIONDESERIALIZE_H
#define TRANSACTIONDESERIALIZE_H

#include <ArduinoOcpp/Model/Transactions/Transaction.h>

#include <ArduinoJson.h>

namespace ArduinoOcpp {

bool serializeTransaction(Transaction& tx, DynamicJsonDocument& out);
bool deserializeTransaction(Transaction& tx, JsonObject in);

}

#endif
