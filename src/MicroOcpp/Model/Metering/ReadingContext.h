// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_READINGCONTEXT_H
#define MO_READINGCONTEXT_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16 || MO_ENABLE_V201

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

typedef enum {
    MO_ReadingContext_UNDEFINED,
    MO_ReadingContext_InterruptionBegin,
    MO_ReadingContext_InterruptionEnd,
    MO_ReadingContext_Other,
    MO_ReadingContext_SampleClock,
    MO_ReadingContext_SamplePeriodic,
    MO_ReadingContext_TransactionBegin,
    MO_ReadingContext_TransactionEnd,
    MO_ReadingContext_Trigger
}   MO_ReadingContext;

#ifdef __cplusplus
} //extern "C"

namespace MicroOcpp {
const char *serializeReadingContext(MO_ReadingContext context);
MO_ReadingContext deserializeReadingContext(const char *serialized);
}

#endif //__cplusplus

#endif //MO_ENABLE_V16 || MO_ENABLE_V201
#endif
