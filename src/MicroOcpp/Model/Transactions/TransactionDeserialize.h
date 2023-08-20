// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef TRANSACTIONDESERIALIZE_H
#define TRANSACTIONDESERIALIZE_H

#include <MicroOcpp/Model/Transactions/Transaction.h>

#include <ArduinoJson.h>

namespace MicroOcpp {

bool serializeTransaction(Transaction& tx, DynamicJsonDocument& out);
bool deserializeTransaction(Transaction& tx, JsonObject in);

}

#endif
