// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Tasks/Metering/SampledValue.h>
#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::SampledValue;

//helper function
namespace ArduinoOcpp {
namespace Ocpp16 {
const char *cstrFromReadingContext(ReadingContext context) {
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
            AO_DBG_ERR("ReadingContext not specified");
        case (ReadingContext::NOT_SET):
            return nullptr;
    }
}

}} //end namespaces

std::unique_ptr<DynamicJsonDocument> SampledValue::toJson() {
    auto value = serializeValue();
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
    auto context_cstr = Ocpp16::cstrFromReadingContext(context);
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
    return std::move(result);
}
