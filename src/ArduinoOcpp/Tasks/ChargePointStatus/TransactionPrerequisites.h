// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef TXPREREQUISITES_H
#define TXPREREQUISITES_H

namespace ArduinoOcpp {

/*
 * Type definitions for extending the transaction initiation process
 */

enum class TxPrecondition {
    Active,
    Inactive
};

enum class TxTrigger {
    Active,
    Inactive
};

enum class TxEnableState {
    Active,
    Inactive,
    Pending
};

}

#endif
