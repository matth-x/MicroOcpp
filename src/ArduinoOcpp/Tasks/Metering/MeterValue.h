// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef METERVALUE_H
#define METERVALUE_H

#include <ArduinoOcpp/Core/OcppTime.h>
#include <ArduinoOcpp/Tasks/Metering/SampledValue.h>
#include <ArduinoJson.h>
#include <memory>

namespace ArduinoOcpp {

class MeterValue {
private:
    OcppTimestamp timestamp;
    std::vector<std::unique_ptr<SampledValue>> sampledValue;
public:
    MeterValue(OcppTimestamp timestamp) : timestamp(timestamp) { }
    MeterValue(const MeterValue& other) {
        timestamp = other.timestamp;
        for (auto value = other.sampledValue.begin(); value != other.sampledValue.end(); value++) {
            sampledValue.push_back(std::unique_ptr<SampledValue>((*value)->clone()));
        }
    }

    void addSampledValue(std::unique_ptr<SampledValue> sample) {sampledValue.push_back(std::move(sample));}

    std::unique_ptr<DynamicJsonDocument> toJson() {
        size_t capacity = 0;
        std::vector<std::unique_ptr<DynamicJsonDocument>> entries;
        for (auto sample = sampledValue.begin(); sample != sampledValue.end(); sample++) {
            auto json = (*sample)->toJson();
            capacity += json->capacity();
            entries.push_back(std::move(json));
        }

        capacity += JSON_ARRAY_SIZE(entries.size());
        capacity += JSONDATE_LENGTH + 1;
        capacity += JSON_OBJECT_SIZE(2);
        
        auto result = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(capacity + 100)); //TODO remove safety space
        auto jsonPayload = result->to<JsonObject>();

        char timestampStr [JSONDATE_LENGTH + 1] = {'\0'};
        if (!timestamp.toJsonString(timestampStr, JSONDATE_LENGTH + 1)) {
            return nullptr;
        }
        jsonPayload["timestamp"] = timestampStr;
        auto jsonMeterValue = jsonPayload.createNestedArray("sampledValue");
        for (auto entry = entries.begin(); entry != entries.end(); entry++) {
            jsonMeterValue.add(**entry);
        }
        return std::move(result);
    }
};

}

#endif
