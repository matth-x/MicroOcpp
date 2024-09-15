// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_READINGCONTEXT_H
#define MO_READINGCONTEXT_H

typedef enum {
    ReadingContext_UNDEFINED,
    ReadingContext_InterruptionBegin,
    ReadingContext_InterruptionEnd,
    ReadingContext_Other,
    ReadingContext_SampleClock,
    ReadingContext_SamplePeriodic,
    ReadingContext_TransactionBegin,
    ReadingContext_TransactionEnd,
    ReadingContext_Trigger
}   ReadingContext;

#ifdef __cplusplus

namespace MicroOcpp {
const char *serializeReadingContext(ReadingContext context);
ReadingContext deserializeReadingContext(const char *serialized);
}

#endif
#endif
