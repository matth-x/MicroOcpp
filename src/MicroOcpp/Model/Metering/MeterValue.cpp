// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Model/Metering/MeterValue.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

std::unique_ptr<DynamicJsonDocument> MeterValue::toJson() {
    size_t capacity = 0;
    std::vector<std::unique_ptr<DynamicJsonDocument>> entries;
    for (auto sample = sampledValue.begin(); sample != sampledValue.end(); sample++) {
        auto json = (*sample)->toJson();
        if (!json) {
            return nullptr;
        }
        capacity += json->capacity();
        entries.push_back(std::move(json));
    }

    capacity += JSON_ARRAY_SIZE(entries.size());
    capacity += JSONDATE_LENGTH + 1;
    capacity += JSON_OBJECT_SIZE(2);
    
    auto result = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(capacity + 100)); //TODO remove safety space
    auto jsonPayload = result->to<JsonObject>();

    char timestampStr [JSONDATE_LENGTH + 1] = {'\0'};
    if (timestamp.toJsonString(timestampStr, JSONDATE_LENGTH + 1)) {
        jsonPayload["timestamp"] = timestampStr;
    }
    auto jsonMeterValue = jsonPayload.createNestedArray("sampledValue");
    for (auto entry = entries.begin(); entry != entries.end(); entry++) {
        jsonMeterValue.add(**entry);
    }
    return result;
}

const Timestamp& MeterValue::getTimestamp() {
    return timestamp;
}

void MeterValue::setTimestamp(Timestamp timestamp) {
    this->timestamp = timestamp;
}

ReadingContext MeterValue::getReadingContext() {
    //all sampledValues have the same ReadingContext. Just get the first result
    for (auto sample = sampledValue.begin(); sample != sampledValue.end(); sample++) {
        if ((*sample)->getReadingContext() != ReadingContext::NOT_SET) {
            return (*sample)->getReadingContext();
        }
    }
    return ReadingContext::NOT_SET;
}

MeterValueBuilder::MeterValueBuilder(const std::vector<std::unique_ptr<SampledValueSampler>> &samplers,
            std::shared_ptr<Configuration> samplersSelectStr) :
            samplers(samplers),
            selectString(samplersSelectStr) {
        
    updateObservedSamplers();
    select_observe = selectString->getValueRevision();
}

void MeterValueBuilder::updateObservedSamplers() {

    if (select_mask.size() != samplers.size()) {
        select_mask.resize(samplers.size(), false);
        select_n = 0;
    }

    for (size_t i = 0; i < select_mask.size(); i++) {
        select_mask[i] = false;
    }
    
    auto sstring = selectString->getString();
    auto ssize = strlen(sstring) + 1;
    size_t sl = 0, sr = 0;
    while (sstring && sl < ssize) {
        while (sr < ssize) {
            if (sstring[sr] == ',') {
                break;
            }
            sr++;
        }

        if (sr != sl + 1) {
            for (size_t i = 0; i < samplers.size(); i++) {
                if (!strncmp(samplers[i]->getProperties().getMeasurand().c_str(), sstring + sl, sr - sl)) {
                    select_mask[i] = true;
                    select_n++;
                }
            }
        }

        sr++;
        sl = sr;
    }
}

std::unique_ptr<MeterValue> MeterValueBuilder::takeSample(const Timestamp& timestamp, const ReadingContext& context) {
    if (select_observe != selectString->getValueRevision() || //OCPP server has changed configuration about which measurands to take
            samplers.size() != select_mask.size()) {    //Client has added another Measurand; synchronize lists
        MO_DBG_DEBUG("Updating observed samplers due to config change or samplers added");
        updateObservedSamplers();
        select_observe = selectString->getValueRevision();
    }

    if (select_n == 0) {
        return nullptr;
    }

    auto sample = std::unique_ptr<MeterValue>(new MeterValue(timestamp));

    for (size_t i = 0; i < select_mask.size(); i++) {
        if (select_mask[i]) {
            sample->addSampledValue(samplers[i]->takeValue(context));
        }
    }

    return sample;
}

std::unique_ptr<MeterValue> MeterValueBuilder::deserializeSample(const JsonObject mvJson) {

    Timestamp timestamp;
    bool ret = timestamp.setTime(mvJson["timestamp"] | "Invalid");
    if (!ret) {
        MO_DBG_ERR("invalid timestamp");
        return nullptr;
    }

    auto sample = std::unique_ptr<MeterValue>(new MeterValue(timestamp));

    JsonArray sampledValue = mvJson["sampledValue"];
    for (JsonObject svJson : sampledValue) {  //for each sampled value, search sampler with matching measurand type
        for (auto& sampler : samplers) {
            auto& properties = sampler->getProperties();
            if (!properties.getMeasurand().compare(svJson["measurand"] | "") &&
                    !properties.getFormat().compare(svJson["format"] | "") &&
                    !properties.getPhase().compare(svJson["phase"] | "") &&
                    !properties.getLocation().compare(svJson["location"] | "") &&
                    !properties.getUnit().compare(svJson["unit"] | "")) {
                //found correct sampler
                auto dVal = sampler->deserializeValue(svJson);
                if (dVal) {
                    sample->addSampledValue(std::move(dVal));
                } else {
                    MO_DBG_ERR("deserialization error");
                }
                break;
            }
        }
    }

    MO_DBG_VERBOSE("deserialized MV");
    return sample;
}
