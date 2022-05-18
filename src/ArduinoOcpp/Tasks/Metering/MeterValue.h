// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef METERVALUE_H
#define METERVALUE_H

#include <ArduinoOcpp/Core/OcppTime.h>
#include <ArduinoOcpp/Tasks/Metering/SampledValue.h>
#include <ArduinoOcpp/Core/ConfigurationKeyValue.h>
#include <ArduinoJson.h>
#include <memory>

namespace ArduinoOcpp {

class MeterValue {
private:
    OcppTimestamp timestamp;
    std::vector<std::unique_ptr<SampledValue>> sampledValue;
public:
    MeterValue(OcppTimestamp timestamp) : timestamp(timestamp) { }
    MeterValue(const MeterValue& other) = delete;

    void addSampledValue(std::unique_ptr<SampledValue> sample) {sampledValue.push_back(std::move(sample));}

    std::unique_ptr<DynamicJsonDocument> toJson();
};

class MeterValueBuilder {
private:
    const std::vector<std::unique_ptr<SampledValueSampler>> &samplers;
    std::shared_ptr<Configuration<const char*>> select;
    std::vector<bool> select_mask;
    unsigned int select_n {0};
    decltype(select->getValueRevision()) select_observe;

    void updateObservedSamplers();
public:
    MeterValueBuilder(const std::vector<std::unique_ptr<SampledValueSampler>> &samplers,
            std::shared_ptr<Configuration<const char*>> samplers_select);
    
    std::unique_ptr<MeterValue> takeSample(const OcppTimestamp& timestamp, const ReadingContext& context);
};

}

#endif
