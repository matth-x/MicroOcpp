// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_METERVALUE_H
#define MO_METERVALUE_H

#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Model/Metering/SampledValue.h>
#include <MicroOcpp/Core/ConfigurationKeyValue.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Version.h>
#include <ArduinoJson.h>
#include <memory>

namespace MicroOcpp {

class MeterValue : public MemoryManaged {
private:
    Timestamp timestamp;
    Vector<std::unique_ptr<SampledValue>> sampledValue;

    int txNr = -1;
    unsigned int opNr = 1;
    unsigned int attemptNr = 0;
    unsigned long attemptTime = 0;
public:
    MeterValue(const Timestamp& timestamp);
    MeterValue(const MeterValue& other) = delete;

    void addSampledValue(std::unique_ptr<SampledValue> sample);

    std::unique_ptr<JsonDoc> toJson();

    const Timestamp& getTimestamp();
    void setTimestamp(Timestamp timestamp);

    ReadingContext getReadingContext();

    void setTxNr(unsigned int txNr);
    int getTxNr();

    void setOpNr(unsigned int opNr);
    unsigned int getOpNr();

    void advanceAttemptNr();
    unsigned int getAttemptNr();

    unsigned long getAttemptTime();
    void setAttemptTime(unsigned long timestamp);
};

#if MO_ENABLE_V201
class Variable;
#endif

class MeterValueBuilder : public MemoryManaged {
private:
    const Vector<std::unique_ptr<SampledValueSampler>> &samplers;

    std::shared_ptr<Configuration> selectString;
    #if MO_ENABLE_V201
    Variable *selectString_v201 = nullptr;
    #endif
    const char *getSelectStringValueCompat();
    unsigned int getSelectStringValueRevisionCompat();

    Vector<bool> select_mask;
    unsigned int select_n {0};
    decltype(selectString->getValueRevision()) select_observe;

    void updateObservedSamplers();
public:
    MeterValueBuilder(const Vector<std::unique_ptr<SampledValueSampler>> &samplers,
            std::shared_ptr<Configuration> samplersSelectStr);

    #if MO_ENABLE_V201
    MeterValueBuilder(const Vector<std::unique_ptr<SampledValueSampler>> &samplers,
            Variable *samplersSelectStr_v201);
    #endif

    std::unique_ptr<MeterValue> takeSample(const Timestamp& timestamp, const ReadingContext& context);

    static std::unique_ptr<MeterValue> deserializeSample(Vector<std::unique_ptr<SampledValueSampler>>& samplers, const JsonObject mvJson);
};

}

#endif
