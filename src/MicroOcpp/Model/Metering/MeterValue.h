// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_METERVALUE_H
#define MO_METERVALUE_H

#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Model/Metering/SampledValue.h>
#include <MicroOcpp/Core/ConfigurationKeyValue.h>
#include <MicroOcpp/Core/Memory.h>
#include <ArduinoJson.h>
#include <memory>
#include <vector>

namespace MicroOcpp {

class MeterValue : public AllocOverrider {
private:
    Timestamp timestamp;
    MemVector<std::unique_ptr<SampledValue>> sampledValue;
public:
    MeterValue(const Timestamp& timestamp);
    MeterValue(const MeterValue& other) = delete;

    void addSampledValue(std::unique_ptr<SampledValue> sample);

    std::unique_ptr<MemJsonDoc> toJson();

    const Timestamp& getTimestamp();
    void setTimestamp(Timestamp timestamp);

    ReadingContext getReadingContext();
};

class MeterValueBuilder : public AllocOverrider {
private:
    const MemVector<std::unique_ptr<SampledValueSampler>> &samplers;
    std::shared_ptr<Configuration> selectString;
    MemVector<bool> select_mask;
    unsigned int select_n {0};
    decltype(selectString->getValueRevision()) select_observe;

    void updateObservedSamplers();
public:
    MeterValueBuilder(const MemVector<std::unique_ptr<SampledValueSampler>> &samplers,
            std::shared_ptr<Configuration> samplersSelectStr);
    
    std::unique_ptr<MeterValue> takeSample(const Timestamp& timestamp, const ReadingContext& context);

    std::unique_ptr<MeterValue> deserializeSample(const JsonObject mvJson);
};

}

#endif
