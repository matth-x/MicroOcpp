// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Model/Metering/SampledValue.h>
#include <MicroOcpp/Debug.h>
#include <cinttypes>

using namespace MicroOcpp;

int32_t SampledValueDeSerializer<int32_t>::deserialize(const char *str) {
    return strtol(str, nullptr, 10);
}

std::string SampledValueDeSerializer<int32_t>::serialize(int32_t& val) {
    char str [12] = {'\0'};
    snprintf(str, 12, "%" PRId32, val);
    return std::string(str);
}

//helper function
namespace MicroOcpp {
namespace Ocpp16 {
const char *serializeReadingContext(ReadingContext context) {
    switch (context) {
        case (ReadingContext::InterruptionBegin):
            return "Interruption.Begin";
        case (ReadingContext::InterruptionEnd):
            return "Interruption.End";
        case (ReadingContext::Other):
            return "Other";
        case (ReadingContext::SampleClock):
            return "Sample.Clock";
        case (ReadingContext::SamplePeriodic):
            return "Sample.Periodic";
        case (ReadingContext::TransactionBegin):
            return "Transaction.Begin";
        case (ReadingContext::TransactionEnd):
            return "Transaction.End";
        case (ReadingContext::Trigger):
            return "Trigger";
        default:
            MO_DBG_ERR("ReadingContext not specified");
            /* fall through */
        case (ReadingContext::NOT_SET):
            return nullptr;
    }
}
ReadingContext deserializeReadingContext(const char *context) {
    if (!context) {
        MO_DBG_ERR("Invalid argument");
        return ReadingContext::NOT_SET;
    }

    if (!strcmp(context, "NOT_SET")) {
        MO_DBG_DEBUG("Deserialize Null-ReadingContext");
        return ReadingContext::NOT_SET;
    } else if (!strcmp(context, "Sample.Periodic")) {
        return ReadingContext::SamplePeriodic;
    } else if (!strcmp(context, "Sample.Clock")) {
        return ReadingContext::SampleClock;
    } else if (!strcmp(context, "Transaction.Begin")) {
        return ReadingContext::TransactionBegin;
    } else if (!strcmp(context, "Transaction.End")) {
        return ReadingContext::TransactionEnd;
    } else if (!strcmp(context, "Other")) {
        return ReadingContext::Other;
    } else if (!strcmp(context, "Interruption.Begin")) {
        return ReadingContext::InterruptionBegin;
    } else if (!strcmp(context, "Interruption.End")) {
        return ReadingContext::InterruptionEnd;
    } else if (!strcmp(context, "Trigger")) {
        return ReadingContext::Trigger;
    }

    MO_DBG_ERR("ReadingContext not specified %.10s", context);
    return ReadingContext::NOT_SET;
}
}} //end namespaces

std::unique_ptr<DynamicJsonDocument> SampledValue::toJson() {
    auto value = serializeValue();
    if (value.empty()) {
        return nullptr;
    }
    size_t capacity = 0;
    capacity += JSON_OBJECT_SIZE(8);
    capacity += value.length() + 1
                + properties.getFormat().length() + 1
                + properties.getMeasurand().length() + 1
                + properties.getPhase().length() + 1
                + properties.getLocation().length() + 1
                + properties.getUnit().length() + 1;
    auto result = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(capacity + 100)); //TODO remove safety space
    auto payload = result->to<JsonObject>();
    payload["value"] = value;
    auto context_cstr = Ocpp16::serializeReadingContext(context);
    if (context_cstr)
        payload["context"] = context_cstr;
    if (!properties.getFormat().empty())
        payload["format"] = properties.getFormat();
    if (!properties.getMeasurand().empty())
        payload["measurand"] = properties.getMeasurand();
    if (!properties.getPhase().empty())
        payload["phase"] = properties.getPhase();
    if (!properties.getLocation().empty())
        payload["location"] = properties.getLocation();
    if (!properties.getUnit().empty())
        payload["unit"] = properties.getUnit();
    return result;
}

ReadingContext SampledValue::getReadingContext() {
    return context;
}
