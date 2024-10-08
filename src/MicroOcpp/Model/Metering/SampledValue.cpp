// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Metering/SampledValue.h>
#include <MicroOcpp/Debug.h>
#include <cinttypes>

#ifndef MO_SAMPLEDVALUE_FLOAT_FORMAT
#define MO_SAMPLEDVALUE_FLOAT_FORMAT "%.2f"
#endif

using namespace MicroOcpp;

int32_t SampledValueDeSerializer<int32_t>::deserialize(const char *str) {
    return strtol(str, nullptr, 10);
}

MicroOcpp::String SampledValueDeSerializer<int32_t>::serialize(int32_t& val) {
    char str [12] = {'\0'};
    snprintf(str, 12, "%" PRId32, val);
    return makeString("v16.Metering.SampledValueDeSerializer<int32_t>", str);
}

MicroOcpp::String SampledValueDeSerializer<float>::serialize(float& val) {
    char str [20];
    str[0] = '\0';
    snprintf(str, 20, MO_SAMPLEDVALUE_FLOAT_FORMAT, val);
    return makeString("v16.Metering.SampledValueDeSerializer<float>", str);
}

std::unique_ptr<JsonDoc> SampledValue::toJson() {
    auto value = serializeValue();
    if (value.empty()) {
        return nullptr;
    }
    size_t capacity = 0;
    capacity += JSON_OBJECT_SIZE(8);
    capacity += value.length() + 1;
    auto result = makeJsonDoc("v16.Metering.SampledValue", capacity);
    auto payload = result->to<JsonObject>();
    payload["value"] = value;
    auto context_cstr = serializeReadingContext(context);
    if (context_cstr)
        payload["context"] = context_cstr;
    if (*properties.getFormat())
        payload["format"] = properties.getFormat();
    if (*properties.getMeasurand())
        payload["measurand"] = properties.getMeasurand();
    if (*properties.getPhase())
        payload["phase"] = properties.getPhase();
    if (*properties.getLocation())
        payload["location"] = properties.getLocation();
    if (*properties.getUnit())
        payload["unit"] = properties.getUnit();
    return result;
}

ReadingContext SampledValue::getReadingContext() {
    return context;
}
