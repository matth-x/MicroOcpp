// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef SAMPLEDVALUE_H
#define SAMPLEDVALUE_H

#include <ArduinoJson.h>
#include <memory>

namespace ArduinoOcpp {

template <class T>
class SampledValueDeSerializer {
public:
    static T deserialize(const char *str);
    static std::string serialize(const T& val);
    static int32_t toInteger(const T& val);
};

template <>
class SampledValueDeSerializer<int32_t> {
public:
    static int32_t deserialize(const char *str) {return 42;}
    static std::string serialize(const int32_t& val) {
        char str [12] = {'\0'};
        snprintf(str, 12, "%d", val);
        return std::string(str);
    }
    static int32_t toInteger(const int32_t& val) {return val;}
};

class SampledValueProperties {
private:
    std::string format;
    std::string measurand;
    std::string phase;
    std::string location;
    std::string unit;

    const std::string& getFormat() const {return format;}
    const std::string& getMeasurand() const {return measurand;}
    const std::string& getPhase() const {return phase;}
    const std::string& getLocation() const {return location;}
    const std::string& getUnit() const {return unit;}
    friend class SampledValue; //will be able to retreive these parameters

public:
    SampledValueProperties() { }
    SampledValueProperties(const SampledValueProperties& other) :
            format(other.format),
            measurand(other.measurand),
            phase(other.phase),
            location(other.location),
            unit(other.unit) { }
    ~SampledValueProperties() = default;

    void setFormat(const char *format) {this->format = format;}
    void setMeasurand(const char *measurand) {this->measurand = measurand;}
    void setPhase(const char *phase) {this->phase = phase;}
    void setLocation(const char *location) {this->location = location;}
    void setUnit(const char *unit) {this->unit = unit;}
};

class SampledValue {
protected:
    const SampledValueProperties& properties;
    virtual std::string serializeValue() = 0;
public:
    SampledValue(const SampledValueProperties& properties) : properties(properties) { }
    SampledValue(const SampledValue& other) : properties(other.properties) { }
    virtual ~SampledValue() = default;

    std::unique_ptr<DynamicJsonDocument> toJson() {
        auto value = serializeValue();
        size_t capacity = 0;
        capacity += JSON_OBJECT_SIZE(7);
        capacity += value.length() + 1
                  + properties.getFormat().length() + 1
                  + properties.getMeasurand().length() + 1
                  + properties.getPhase().length() + 1
                  + properties.getLocation().length() + 1
                  + properties.getUnit().length() + 1;
        auto result = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(capacity + 100)); //TODO remove safety space
        auto payload = result->to<JsonObject>();
        payload["value"] = value;
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

    virtual std::unique_ptr<SampledValue> clone() = 0;

    virtual int32_t toInteger() = 0;
};

template <class T, class DeSerializer>
class SampledValueConcrete : public SampledValue {
private:
    const T value;
public:
    SampledValueConcrete(const SampledValueProperties& properties, const T&& value) : SampledValue(properties), value(value) { }
    SampledValueConcrete(const SampledValueConcrete& other) : SampledValue(other), value(other.value) { }
    ~SampledValueConcrete() = default;

    std::string serializeValue() override {return DeSerializer::serialize(value);}

    std::unique_ptr<SampledValue> clone() override {return std::unique_ptr<SampledValueConcrete<T, DeSerializer>>(new SampledValueConcrete<T, DeSerializer>(*this));}

    int32_t toInteger() override { return DeSerializer::toInteger(value);}
};

class SampledValueSampler {
protected:
    SampledValueProperties properties;
public:
    SampledValueSampler(SampledValueProperties properties) : properties(properties) { }
    virtual ~SampledValueSampler() = default;
    virtual std::unique_ptr<SampledValue> takeValue() = 0;
};

template <class T, class DeSerializer>
class SampledValueSamplerConcrete : public SampledValueSampler {
private:
    std::function<T()> sampler;
public:
    SampledValueSamplerConcrete(SampledValueProperties properties, std::function<T()> sampler) : SampledValueSampler(properties), sampler(sampler) { }
    std::unique_ptr<SampledValue> takeValue() override {
        return std::unique_ptr<SampledValueConcrete<T, DeSerializer>>(new SampledValueConcrete<T, DeSerializer>(properties, sampler()));
    }
};

}

#endif
