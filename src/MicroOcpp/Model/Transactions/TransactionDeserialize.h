// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_TRANSACTIONDESERIALIZE_H
#define MO_TRANSACTIONDESERIALIZE_H

#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Core/Memory.h>

#include <ArduinoJson.h>

namespace MicroOcpp {

bool serializeTransaction(Transaction& tx, MemJsonDoc& out);
bool deserializeTransaction(Transaction& tx, JsonObject in);

}

#endif
