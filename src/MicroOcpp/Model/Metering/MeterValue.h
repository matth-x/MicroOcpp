// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef METERVALUE_H
#define METERVALUE_H

#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Model/Metering/SampledValue.h>
#include <MicroOcpp/Core/ConfigurationKeyValue.h>
#include <ArduinoJson.h>
#include <memory>
#include <vector>

namespace MicroOcpp {

class MeterValue {
private:
    Timestamp timestamp;
    std::vector<std::unique_ptr<SampledValue>> sampledValue;
public:
    MeterValue(Timestamp timestamp) : timestamp(timestamp) { }
    MeterValue(const MeterValue& other) = delete;

    void addSampledValue(std::unique_ptr<SampledValue> sample) {sampledValue.push_back(std::move(sample));}

    std::unique_ptr<DynamicJsonDocument> toJson();

    const Timestamp& getTimestamp();
    void setTimestamp(Timestamp timestamp);

    ReadingContext getReadingContext();
};

class MeterValueBuilder {
private:
    const std::vector<std::unique_ptr<SampledValueSampler>> &samplers;
    std::shared_ptr<Configuration> selectString;
    std::vector<bool> select_mask;
    unsigned int select_n {0};
    decltype(selectString->getValueRevision()) select_observe;

    void updateObservedSamplers();
public:
    MeterValueBuilder(const std::vector<std::unique_ptr<SampledValueSampler>> &samplers,
            std::shared_ptr<Configuration> samplersSelectStr);
    
    std::unique_ptr<MeterValue> takeSample(const Timestamp& timestamp, const ReadingContext& context);

    std::unique_ptr<MeterValue> deserializeSample(const JsonObject mvJson);
};

}

#endif
