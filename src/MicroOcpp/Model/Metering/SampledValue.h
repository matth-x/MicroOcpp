// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef SAMPLEDVALUE_H
#define SAMPLEDVALUE_H

#include <ArduinoJson.h>
#include <memory>
#include <functional>

#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Platform.h>

namespace MicroOcpp {

template <class T>
class SampledValueDeSerializer {
public:
    static T deserialize(const char *str);
    static bool ready(T& val);
    static MemString serialize(T& val);
    static int32_t toInteger(T& val);
};

template <>
class SampledValueDeSerializer<int32_t> { // example class
public:
    static int32_t deserialize(const char *str);
    static bool ready(int32_t& val) {return true;} //int32_t is always valid
    static MemString serialize(int32_t& val);
    static int32_t toInteger(int32_t& val) {return val;} //no conversion required
};

template <>
class SampledValueDeSerializer<float> { // Used in meterValues
public:
    static float deserialize(const char *str) {return atof(str);}
    static bool ready(float& val) {return true;} //float is always valid
    static MemString serialize(float& val);
    static int32_t toInteger(float& val) {return (int32_t) val;}
};

class SampledValueProperties {
private:
    MemString format;
    MemString measurand;
    MemString phase;
    MemString location;
    MemString unit;

public:
    SampledValueProperties() :
            format(makeMemString("v16.Metering.SampledValueProperties")),
            measurand(makeMemString("v16.Metering.SampledValueProperties")),
            phase(makeMemString("v16.Metering.SampledValueProperties")),
            location(makeMemString("v16.Metering.SampledValueProperties")),
            unit(makeMemString("v16.Metering.SampledValueProperties")) { }
    SampledValueProperties(const SampledValueProperties& other) :
            format(other.format),
            measurand(other.measurand),
            phase(other.phase),
            location(other.location),
            unit(other.unit) { }
    ~SampledValueProperties() = default;

    void setFormat(const char *format) {this->format = format;}
    const char *getFormat() const {return format.c_str();}
    void setMeasurand(const char *measurand) {this->measurand = measurand;}
    const char *getMeasurand() const {return measurand.c_str();}
    void setPhase(const char *phase) {this->phase = phase;}
    const char *getPhase() const {return phase.c_str();}
    void setLocation(const char *location) {this->location = location;}
    const char *getLocation() const {return location.c_str();}
    void setUnit(const char *unit) {this->unit = unit;}
    const char *getUnit() const {return unit.c_str();}
};

enum class ReadingContext {
    InterruptionBegin,
    InterruptionEnd,
    Other,
    SampleClock,
    SamplePeriodic,
    TransactionBegin,
    TransactionEnd,
    Trigger,
    NOT_SET
};

namespace Ocpp16 {
const char *serializeReadingContext(ReadingContext context);
ReadingContext deserializeReadingContext(const char *serialized);
}

class SampledValue {
protected:
    const SampledValueProperties& properties;
    const ReadingContext context;
    virtual MemString serializeValue() = 0;
public:
    SampledValue(const SampledValueProperties& properties, ReadingContext context) : properties(properties), context(context) { }
    SampledValue(const SampledValue& other) : properties(other.properties), context(other.context) { }
    virtual ~SampledValue() = default;

    std::unique_ptr<MemJsonDoc> toJson();

    virtual operator bool() = 0;
    virtual int32_t toInteger() = 0;

    ReadingContext getReadingContext();
};

template <class T, class DeSerializer>
class SampledValueConcrete : public SampledValue, public AllocOverrider {
private:
    T value;
public:
    SampledValueConcrete(const SampledValueProperties& properties, ReadingContext context, const T&& value) : SampledValue(properties, context), AllocOverrider("v16.Metering.SampledValueConcrete"), value(value) { }
    SampledValueConcrete(const SampledValueConcrete& other) : SampledValue(other), AllocOverrider(other), value(other.value) { }
    ~SampledValueConcrete() = default;

    operator bool() override {return DeSerializer::ready(value);}

    MemString serializeValue() override {return DeSerializer::serialize(value);}

    int32_t toInteger() override { return DeSerializer::toInteger(value);}
};

class SampledValueSampler {
protected:
    SampledValueProperties properties;
public:
    SampledValueSampler(SampledValueProperties properties) : properties(properties) { }
    virtual ~SampledValueSampler() = default;
    virtual std::unique_ptr<SampledValue> takeValue(ReadingContext context) = 0;
    virtual std::unique_ptr<SampledValue> deserializeValue(JsonObject svJson) = 0;
    const SampledValueProperties& getProperties() {return properties;};
};

template <class T, class DeSerializer>
class SampledValueSamplerConcrete : public SampledValueSampler, public AllocOverrider {
private:
    std::function<T(ReadingContext context)> sampler;
public:
    SampledValueSamplerConcrete(SampledValueProperties properties, std::function<T(ReadingContext)> sampler) : SampledValueSampler(properties), AllocOverrider("v16.Metering.SampledValueSamplerConcrete"), sampler(sampler) { }
    std::unique_ptr<SampledValue> takeValue(ReadingContext context) override {
        return std::unique_ptr<SampledValueConcrete<T, DeSerializer>>(new SampledValueConcrete<T, DeSerializer>(
            properties,
            context,
            sampler(context)));
    }
    std::unique_ptr<SampledValue> deserializeValue(JsonObject svJson) override {
        return std::unique_ptr<SampledValueConcrete<T, DeSerializer>>(new SampledValueConcrete<T, DeSerializer>(
            properties,
            Ocpp16::deserializeReadingContext(svJson["context"] | "NOT_SET"),
            DeSerializer::deserialize(svJson["value"] | "")));
    }
};

} //end namespace MicroOcpp

#endif
