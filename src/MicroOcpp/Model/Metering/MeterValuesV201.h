// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

/*
 * Implementation of the UCs E01 - E12
 */

#ifndef MO_METERVALUESV201_H
#define MO_METERVALUESV201_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <functional>

#include <MicroOcpp/Model/Metering/ReadingContext.h>
#include <MicroOcpp/Model/ConnectorBase/EvseId.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Core/Memory.h>

namespace MicroOcpp {

class Model;
class Variable;

namespace Ocpp201 {

class SampledValueProperties : public MemoryManaged {
private:
    const char *format = nullptr;
    const char *measurand = nullptr;
    const char *phase = nullptr;
    const char *location = nullptr;
    const char *unitOfMeasureUnit = nullptr;
    int unitOfMeasureMultiplier = 0;

public:
    SampledValueProperties();
    SampledValueProperties(const SampledValueProperties&);

    void setFormat(const char *format); //zero-copy
    const char *getFormat() const;
    void setMeasurand(const char *measurand); //zero-copy
    const char *getMeasurand() const;
    void setPhase(const char *phase); //zero-copy
    const char *getPhase() const;
    void setLocation(const char *location); //zero-copy
    const char *getLocation() const;
    void setUnitOfMeasureUnit(const char *unitOfMeasureUnit); //zero-copy
    const char *getUnitOfMeasureUnit() const;
    void setUnitOfMeasureMultiplier(int unitOfMeasureMultiplier);
    int getUnitOfMeasureMultiplier() const;
};

class SampledValue : public MemoryManaged {
private:
    double value = 0.;
    ReadingContext readingContext;
    SampledValueProperties& properties;
    //std::unique_ptr<SignedMeterValue> ... this could be an abstract type
public:
    SampledValue(double value, ReadingContext readingContext, SampledValueProperties& properties);

    bool toJson(JsonDoc& out);
};

#define MO_MEASURAND_TYPE_TXSTARTED (1 << 0)
#define MO_MEASURAND_TYPE_TXUPDATED (1 << 1)
#define MO_MEASURAND_TYPE_TXENDED   (1 << 2)
#define MO_MEASURAND_TYPE_ALIGNED   (1 << 3)

class SampledValueInput : public MemoryManaged {
private:
    std::function<double(ReadingContext)> valueInput;
    SampledValueProperties properties;

    uint8_t measurandTypeFlags = 0;
public:
    SampledValueInput(std::function<double(ReadingContext)> valueInput, const SampledValueProperties& properties);
    SampledValue *takeSampledValue(ReadingContext readingContext);

    const SampledValueProperties& getProperties();

    uint8_t& getMeasurandTypeFlags();
};

class MeterValue : public MemoryManaged {
private:
    Timestamp timestamp;
    SampledValue **sampledValue = nullptr;
    size_t sampledValueSize = 0;
public:
    MeterValue(const Timestamp& timestamp, SampledValue **sampledValue, size_t sampledValueSize);
    ~MeterValue();

    bool toJson(JsonDoc& out);

    const Timestamp& getTimestamp();
};

class MeteringServiceEvse : public MemoryManaged {
private:
    Model& model;
    const unsigned int evseId;
 
    Vector<SampledValueInput> sampledValueInputs;

    Variable *sampledDataTxStartedMeasurands = nullptr;
    Variable *sampledDataTxUpdatedMeasurands = nullptr;
    Variable *sampledDataTxEndedMeasurands = nullptr;
    Variable *alignedDataMeasurands = nullptr;

    size_t trackSampledValueInputsSizeTxStarted = 0;
    size_t trackSampledValueInputsSizeTxUpdated = 0;
    size_t trackSampledValueInputsSizeTxEnded = 0;
    size_t trackSampledValueInputsSizeAligned = 0;
    uint16_t trackSampledDataTxStartedMeasurandsWriteCount = -1;
    uint16_t trackSampledDataTxUpdatedMeasurandsWriteCount = -1;
    uint16_t trackSampledDataTxEndedMeasurandsWriteCount = -1;
    uint16_t trackAlignedDataMeasurandsWriteCount = -1;

    std::unique_ptr<MeterValue> takeMeterValue(Variable *measurands, uint16_t& trackMeasurandsWriteCount, size_t& trackInputsSize, uint8_t measurandsMask, ReadingContext context);
public:
    MeteringServiceEvse(Model& model, unsigned int evseId);

    void addMeterValueInput(std::function<double(ReadingContext)> valueInput, const SampledValueProperties& properties);

    std::unique_ptr<MeterValue> takeTxStartedMeterValue(ReadingContext context = ReadingContext_TransactionBegin);
    std::unique_ptr<MeterValue> takeTxUpdatedMeterValue(ReadingContext context = ReadingContext_SamplePeriodic);
    std::unique_ptr<MeterValue> takeTxEndedMeterValue(ReadingContext context);
    std::unique_ptr<MeterValue> takeTriggeredMeterValues();

    bool existsMeasurand(const char *measurand, size_t len);
};

class MeteringService : public MemoryManaged {
private:
    MeteringServiceEvse* evses [MO_NUM_EVSEID] = {nullptr};
public:
    MeteringService(Model& model, size_t numEvses);
    ~MeteringService();

    MeteringServiceEvse *getEvse(unsigned int evseId);
};

}
}

#endif
#endif
