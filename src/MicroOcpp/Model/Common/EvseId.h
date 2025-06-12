// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_EVSEID_H
#define MO_EVSEID_H

#include <MicroOcpp/Version.h>

// number of EVSE IDs (including 0). On a charger with one physical connector, NUM_EVSEID is 2
#ifndef MO_NUM_EVSEID
// Use MO_NUMCONNECTORS if defined, for backwards compatibility
#if defined(MO_NUMCONNECTORS)
#define MO_NUM_EVSEID MO_NUMCONNECTORS
#else
#define MO_NUM_EVSEID 2
#endif
#endif //MO_NUM_EVSEID

#ifndef MO_NUM_EVSEID_DIGITS
#define MO_NUM_EVSEID_DIGITS 1 //digits needed to print MO_NUM_EVSEID-1 (="1", i.e. 1 digit)
#endif

#if MO_ENABLE_V201
#ifdef __cplusplus

namespace MicroOcpp {

// EVSEType (2.23)
struct EvseId {
    int id;
    int connectorId = -1; //optional

    EvseId(int id) : id(id) { }
    EvseId(int id, int connectorId) : id(id), connectorId(connectorId) { }
};

}

#endif //__cplusplus
#endif //MO_ENABLE_V201
#endif
