// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_EVSEID_H
#define MO_EVSEID_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

namespace MicroOcpp {

// EVSEType (2.23)
struct EvseId {
    int id;
    int connectorId = -1; //optional

    EvseId(int id) : id(id) { }
    EvseId(int id, int connectorId) : id(id), connectorId(connectorId) { }
};

}

#endif // MO_ENABLE_V201
#endif
