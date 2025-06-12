// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <string.h>

#include <MicroOcpp/Model/Metering/ReadingContext.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16 || MO_ENABLE_V201

namespace MicroOcpp {

const char *serializeReadingContext(MO_ReadingContext context) {
    switch (context) {
        case (MO_ReadingContext_InterruptionBegin):
            return "Interruption.Begin";
        case (MO_ReadingContext_InterruptionEnd):
            return "Interruption.End";
        case (MO_ReadingContext_Other):
            return "Other";
        case (MO_ReadingContext_SampleClock):
            return "Sample.Clock";
        case (MO_ReadingContext_SamplePeriodic):
            return "Sample.Periodic";
        case (MO_ReadingContext_TransactionBegin):
            return "Transaction.Begin";
        case (MO_ReadingContext_TransactionEnd):
            return "Transaction.End";
        case (MO_ReadingContext_Trigger):
            return "Trigger";
        default:
            MO_DBG_ERR("MO_ReadingContext not specified");
            /* fall through */
        case (MO_ReadingContext_UNDEFINED):
            return "";
    }
}
MO_ReadingContext deserializeReadingContext(const char *context) {
    if (!context) {
        MO_DBG_ERR("Invalid argument");
        return MO_ReadingContext_UNDEFINED;
    }

    if (!strcmp(context, "Sample.Periodic")) {
        return MO_ReadingContext_SamplePeriodic;
    } else if (!strcmp(context, "Sample.Clock")) {
        return MO_ReadingContext_SampleClock;
    } else if (!strcmp(context, "Transaction.Begin")) {
        return MO_ReadingContext_TransactionBegin;
    } else if (!strcmp(context, "Transaction.End")) {
        return MO_ReadingContext_TransactionEnd;
    } else if (!strcmp(context, "Other")) {
        return MO_ReadingContext_Other;
    } else if (!strcmp(context, "Interruption.Begin")) {
        return MO_ReadingContext_InterruptionBegin;
    } else if (!strcmp(context, "Interruption.End")) {
        return MO_ReadingContext_InterruptionEnd;
    } else if (!strcmp(context, "Trigger")) {
        return MO_ReadingContext_Trigger;
    }

    MO_DBG_ERR("ReadingContext not specified %.10s", context);
    return MO_ReadingContext_UNDEFINED;
}

} //namespace MicroOcpp
#endif //MO_ENABLE_V16 || MO_ENABLE_V201
