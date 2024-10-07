// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Model/Metering/MeterValuesV201.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Variables/Variable.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Debug.h>

//helper function
namespace MicroOcpp {

bool csvContains(const char *csv, const char *elem) {

    if (!csv || !elem) {
        return false;
    }

    size_t elemLen = strlen(elem);

    size_t sl = 0, sr = 0;
    while (csv[sr]) {
        while (csv[sr]) {
            if (csv[sr] == ',') {
                break;
            }
            sr++;
        }
        //csv[sr] is either ',' or '\0'

        if (sr - sl == elemLen && !strncmp(csv + sl, elem, sr - sl)) {
            return true;
        }

        if (csv[sr]) {
            sr++;
        }
        sl = sr;
    }
    return false;
}

} //namespace MicroOcpp

using namespace MicroOcpp::Ocpp201;

SampledValueProperties::SampledValueProperties() : MemoryManaged("v201.MeterValues.SampledValueProperties") { }
SampledValueProperties::SampledValueProperties(const SampledValueProperties& other) :
        MemoryManaged(other.getMemoryTag()), 
        format(other.format), 
        measurand(other.measurand), 
        phase(other.phase), 
        location(other.location), 
        unitOfMeasureUnit(other.unitOfMeasureUnit),
        unitOfMeasureMultiplier(other.unitOfMeasureMultiplier) {

}

void SampledValueProperties::setFormat(const char *format) {this->format = format;}
const char *SampledValueProperties::getFormat() const {return format;}
void SampledValueProperties::setMeasurand(const char *measurand) {this->measurand = measurand;}
const char *SampledValueProperties::getMeasurand() const {return measurand;}
void SampledValueProperties::setPhase(const char *phase) {this->phase = phase;}
const char *SampledValueProperties::getPhase() const {return phase;}
void SampledValueProperties::setLocation(const char *location) {this->location = location;}
const char *SampledValueProperties::getLocation() const {return location;}
void SampledValueProperties::setUnitOfMeasureUnit(const char *unitOfMeasureUnit) {this->unitOfMeasureUnit = unitOfMeasureUnit;}
const char *SampledValueProperties::getUnitOfMeasureUnit() const {return unitOfMeasureUnit;}
void SampledValueProperties::setUnitOfMeasureMultiplier(int unitOfMeasureMultiplier) {this->unitOfMeasureMultiplier = unitOfMeasureMultiplier;}
int SampledValueProperties::getUnitOfMeasureMultiplier() const {return unitOfMeasureMultiplier;}

SampledValue::SampledValue(double value, ReadingContext readingContext, SampledValueProperties& properties)
        : MemoryManaged("v201.MeterValues.SampledValue"), value(value), readingContext(readingContext), properties(properties) {

}

bool SampledValue::toJson(JsonDoc& out) {

    size_t unitOfMeasureElements = 
            (properties.getUnitOfMeasureUnit() ? 1 : 0) +
            (properties.getUnitOfMeasureMultiplier() ? 1 : 0);

    out = initJsonDoc(getMemoryTag(),
        JSON_OBJECT_SIZE(
            1 + //value
            (readingContext != ReadingContext_SamplePeriodic ? 1 : 0) +
            (properties.getMeasurand() ? 1 : 0) +
            (properties.getPhase() ? 1 : 0) +
            (properties.getLocation() ? 1 : 0) +
            (unitOfMeasureElements ? 1 : 0)
        ) +
        (unitOfMeasureElements ? JSON_OBJECT_SIZE(unitOfMeasureElements) : 0)
    );

    out["value"] = value;
    if (readingContext != ReadingContext_SamplePeriodic)
        out["context"] = serializeReadingContext(readingContext);
    if (properties.getMeasurand())
        out["measurand"] = properties.getMeasurand();
    if (properties.getPhase())
        out["phase"] = properties.getPhase();
    if (properties.getLocation())
        out["location"] = properties.getLocation();
    if (properties.getUnitOfMeasureUnit())
        out["unitOfMeasure"]["unit"] = properties.getUnitOfMeasureUnit();
    if (properties.getUnitOfMeasureMultiplier())
        out["unitOfMeasure"]["multiplier"] = properties.getUnitOfMeasureMultiplier();

    return true;
}

SampledValueInput::SampledValueInput(std::function<double(ReadingContext)> valueInput, const SampledValueProperties& properties)
        : MemoryManaged("v201.MeterValues.SampledValueInput"), valueInput(valueInput), properties(properties) {

}

SampledValue *SampledValueInput::takeSampledValue(ReadingContext readingContext) {
    return new SampledValue(valueInput(readingContext), readingContext, properties);
}

const SampledValueProperties& SampledValueInput::getProperties() {
    return properties;
}

uint8_t& SampledValueInput::getMeasurandTypeFlags() {
    return measurandTypeFlags;
}

MeterValue::MeterValue(const Timestamp& timestamp, SampledValue **sampledValue, size_t sampledValueSize) :
        MemoryManaged("v201.MeterValues.MeterValue"), timestamp(timestamp), sampledValue(sampledValue), sampledValueSize(sampledValueSize) {
    
}

MeterValue::~MeterValue() {
    for (size_t i = 0; i < sampledValueSize; i++) {
        delete sampledValue[i];
    }
    MO_FREE(sampledValue);
}

bool MeterValue::toJson(JsonDoc& out) {

    size_t capacity = 0;

    for (size_t i = 0; i < sampledValueSize; i++) {
        //just measure, discard sampledValueJson afterwards
        JsonDoc sampledValueJson = initJsonDoc(getMemoryTag());
        sampledValue[i]->toJson(sampledValueJson);
        capacity += sampledValueJson.capacity();
    }

    capacity += JSON_OBJECT_SIZE(2) +
                JSONDATE_LENGTH + 1 +
                JSON_ARRAY_SIZE(sampledValueSize);
                

    out = initJsonDoc("v201.MeterValues.MeterValue", capacity);

    char timestampStr [JSONDATE_LENGTH + 1];
    timestamp.toJsonString(timestampStr, sizeof(timestampStr));
    
    out["timestamp"] = timestampStr;
    JsonArray sampledValueArray = out.createNestedArray("sampledValue");
    
    for (size_t i = 0; i < sampledValueSize; i++) {
        JsonDoc sampledValueJson = initJsonDoc(getMemoryTag());
        sampledValue[i]->toJson(sampledValueJson);
        sampledValueArray.add(sampledValueJson);
    }

    return true;
}

const MicroOcpp::Timestamp& MeterValue::getTimestamp() {
    return timestamp;
}

MeteringServiceEvse::MeteringServiceEvse(Model& model, unsigned int evseId)
        : MemoryManaged("v201.MeterValues.MeteringServiceEvse"), model(model), evseId(evseId), sampledValueInputs(makeVector<SampledValueInput>(getMemoryTag())) {

    auto varService = model.getVariableService();

    sampledDataTxStartedMeasurands = varService->declareVariable<const char*>("SampledDataCtrlr", "TxStartedMeasurands", "");
    sampledDataTxUpdatedMeasurands = varService->declareVariable<const char*>("SampledDataCtrlr", "TxUpdatedMeasurands", "");
    sampledDataTxEndedMeasurands = varService->declareVariable<const char*>("SampledDataCtrlr", "TxEndedMeasurands", "");
    alignedDataMeasurands = varService->declareVariable<const char*>("AlignedDataCtrlr", "AlignedDataMeasurands", "");
}

void MeteringServiceEvse::addMeterValueInput(std::function<double(ReadingContext)> valueInput, const SampledValueProperties& properties) {
    sampledValueInputs.emplace_back(valueInput, properties);
}

std::unique_ptr<MeterValue> MeteringServiceEvse::takeMeterValue(Variable *measurands, uint16_t& trackMeasurandsWriteCount, size_t& trackInputsSize, uint8_t measurandsMask, ReadingContext readingContext) {

    if (measurands->getWriteCount() != trackMeasurandsWriteCount ||
            sampledValueInputs.size() != trackInputsSize) {
        MO_DBG_DEBUG("Updating observed samplers due to config change or samplers added");
        for (size_t i = 0; i < sampledValueInputs.size(); i++) {
            if (csvContains(measurands->getString(), sampledValueInputs[i].getProperties().getMeasurand())) {
                sampledValueInputs[i].getMeasurandTypeFlags() |= measurandsMask;
            } else {
                sampledValueInputs[i].getMeasurandTypeFlags() &= ~measurandsMask;
            }
        }

        trackMeasurandsWriteCount = measurands->getWriteCount();
        trackInputsSize = sampledValueInputs.size();
    }

    size_t samplesSize = 0;

    for (size_t i = 0; i < sampledValueInputs.size(); i++) {
        if (sampledValueInputs[i].getMeasurandTypeFlags() & measurandsMask) {
            samplesSize++;
        }
    }

    if (samplesSize == 0) {
        return nullptr;
    }

    SampledValue **sampledValue = static_cast<SampledValue**>(MO_MALLOC(getMemoryTag(), samplesSize * sizeof(SampledValue*)));
    if (!sampledValue) {
        MO_DBG_ERR("OOM");
        return nullptr;
    }
    size_t samplesWritten = 0;

    bool memoryErr = false;

    for (size_t i = 0; i < sampledValueInputs.size(); i++) {
        if (sampledValueInputs[i].getMeasurandTypeFlags() & measurandsMask) {
            auto sample = sampledValueInputs[i].takeSampledValue(readingContext);
            if (!sample) {
                MO_DBG_ERR("OOM");
                memoryErr = true;
                break;
            }
            sampledValue[samplesWritten++] = sample;
        }
    }

    std::unique_ptr<MeterValue> meterValue = std::unique_ptr<MeterValue>(new MeterValue(model.getClock().now(), sampledValue, samplesWritten));
    if (!meterValue) {
        MO_DBG_ERR("OOM");
        memoryErr = true;
    }

    if (memoryErr) {
        if (!meterValue) {
            //meterValue did not take ownership, so clean resources manually
            for (size_t i = 0; i < samplesWritten; i++) {
                delete sampledValue[i];
            }
            delete sampledValue;
        }
        return nullptr;
    }

    return meterValue;
}

std::unique_ptr<MeterValue> MeteringServiceEvse::takeTxStartedMeterValue(ReadingContext readingContext) {
    return takeMeterValue(sampledDataTxStartedMeasurands, trackSampledDataTxStartedMeasurandsWriteCount, trackSampledValueInputsSizeTxStarted, MO_MEASURAND_TYPE_TXSTARTED, readingContext);
}
std::unique_ptr<MeterValue> MeteringServiceEvse::takeTxUpdatedMeterValue(ReadingContext readingContext) {
    return takeMeterValue(sampledDataTxUpdatedMeasurands, trackSampledDataTxUpdatedMeasurandsWriteCount, trackSampledValueInputsSizeTxUpdated, MO_MEASURAND_TYPE_TXUPDATED, readingContext);
}
std::unique_ptr<MeterValue> MeteringServiceEvse::takeTxEndedMeterValue(ReadingContext readingContext) {
    return takeMeterValue(sampledDataTxEndedMeasurands, trackSampledDataTxEndedMeasurandsWriteCount, trackSampledValueInputsSizeTxEnded, MO_MEASURAND_TYPE_TXENDED, readingContext);
}
std::unique_ptr<MeterValue> MeteringServiceEvse::takeTriggeredMeterValues() {
    return takeMeterValue(alignedDataMeasurands, trackAlignedDataMeasurandsWriteCount, trackSampledValueInputsSizeAligned, MO_MEASURAND_TYPE_ALIGNED, ReadingContext_Trigger);
}

bool MeteringServiceEvse::existsMeasurand(const char *measurand, size_t len) {
    for (size_t i = 0; i < sampledValueInputs.size(); i++) {
        const char *sviMeasurand = sampledValueInputs[i].getProperties().getMeasurand();
        if (sviMeasurand && len == strlen(sviMeasurand) && !strncmp(sviMeasurand, measurand, len)) {
            return true;
        }
    }
    return false;
}

MeteringService::MeteringService(Model& model, size_t numEvses) {

    auto varService = model.getVariableService();

    //define factory defaults
    varService->declareVariable<const char*>("SampledDataCtrlr", "TxStartedMeasurands", "");
    varService->declareVariable<const char*>("SampledDataCtrlr", "TxUpdatedMeasurands", "");
    varService->declareVariable<const char*>("SampledDataCtrlr", "TxEndedMeasurands", "");
    varService->declareVariable<const char*>("AlignedDataCtrlr", "AlignedDataMeasurands", "");

    std::function<bool(const char*)> validateSelectString = [this] (const char *csl) {
        bool isValid = true;
        const char *l = csl; //the beginning of an entry of the comma-separated list
        const char *r = l; //one place after the last character of the entry beginning with l
        while (*l) {
            if (*l == ',') {
                l++;
                continue;
            }
            r = l + 1;
            while (*r != '\0' && *r != ',') {
                r++;
            }
            bool found = false;
            for (size_t evseId = 0; evseId < MO_NUM_EVSEID && evses[evseId]; evseId++) {
                if (evses[evseId]->existsMeasurand(l, (size_t) (r - l))) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                isValid = false;
                MO_DBG_WARN("could not find metering device for %.*s", (int) (r - l), l);
                break;
            }
            l = r;
        }
        return isValid;
    };

    varService->registerValidator<const char*>("SampledDataCtrlr", "TxStartedMeasurands", validateSelectString);
    varService->registerValidator<const char*>("SampledDataCtrlr", "TxUpdatedMeasurands", validateSelectString);
    varService->registerValidator<const char*>("SampledDataCtrlr", "TxEndedMeasurands", validateSelectString);
    varService->registerValidator<const char*>("AlignedDataCtrlr", "AlignedDataMeasurands", validateSelectString);

    for (size_t evseId = 0; evseId < std::min(numEvses, (size_t)MO_NUM_EVSEID); evseId++) {
        evses[evseId] = new MeteringServiceEvse(model, evseId);
    }
}

MeteringService::~MeteringService() {
    for (size_t evseId = 0; evseId < MO_NUM_EVSEID && evses[evseId]; evseId++) {
        delete evses[evseId];
    }
}

MeteringServiceEvse *MeteringService::getEvse(unsigned int evseId) {
    return evses[evseId];
}

#endif
