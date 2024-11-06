// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <string.h>

#include <MicroOcpp/Model/Metering/ReadingContext.h>
#include <MicroOcpp/Debug.h>

namespace MicroOcpp {

const char *serializeReadingContext(ReadingContext context) {
    switch (context) {
        case (ReadingContext_InterruptionBegin):
            return "Interruption.Begin";
        case (ReadingContext_InterruptionEnd):
            return "Interruption.End";
        case (ReadingContext_Other):
            return "Other";
        case (ReadingContext_SampleClock):
            return "Sample.Clock";
        case (ReadingContext_SamplePeriodic):
            return "Sample.Periodic";
        case (ReadingContext_TransactionBegin):
            return "Transaction.Begin";
        case (ReadingContext_TransactionEnd):
            return "Transaction.End";
        case (ReadingContext_Trigger):
            return "Trigger";
        default:
            MO_DBG_ERR("ReadingContext not specified");
            /* fall through */
        case (ReadingContext_UNDEFINED):
            return "";
    }
}
ReadingContext deserializeReadingContext(const char *context) {
    if (!context) {
        MO_DBG_ERR("Invalid argument");
        return ReadingContext_UNDEFINED;
    }

    if (!strcmp(context, "Sample.Periodic")) {
        return ReadingContext_SamplePeriodic;
    } else if (!strcmp(context, "Sample.Clock")) {
        return ReadingContext_SampleClock;
    } else if (!strcmp(context, "Transaction.Begin")) {
        return ReadingContext_TransactionBegin;
    } else if (!strcmp(context, "Transaction.End")) {
        return ReadingContext_TransactionEnd;
    } else if (!strcmp(context, "Other")) {
        return ReadingContext_Other;
    } else if (!strcmp(context, "Interruption.Begin")) {
        return ReadingContext_InterruptionBegin;
    } else if (!strcmp(context, "Interruption.End")) {
        return ReadingContext_InterruptionEnd;
    } else if (!strcmp(context, "Trigger")) {
        return ReadingContext_Trigger;
    }

    MO_DBG_ERR("ReadingContext not specified %.10s", context);
    return ReadingContext_UNDEFINED;
}

} //namespace MicroOcpp
