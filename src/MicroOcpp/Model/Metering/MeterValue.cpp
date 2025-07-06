// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Metering/MeterValue.h>
#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16 || MO_ENABLE_V201

void mo_MeterInput_init(MO_MeterInput *mInput, MO_MeterInputType type) {
    memset(mInput, 0, sizeof(*mInput));
    mo_MeterInput_setType(mInput, type);
}

MO_MeterInputType mo_MeterInput_getType(MO_MeterInput *mInput) {
    return static_cast<MO_MeterInputType>(mInput->type);
}
void mo_MeterInput_setType(MO_MeterInput *mInput, MO_MeterInputType type) {
    mInput->type = static_cast<decltype(mInput->type)>(type);
}

using namespace MicroOcpp;

SampledValue::SampledValue(MO_MeterInput& meterInput) : MemoryManaged("v16.Metering.MeterValue"), meterInput(meterInput) {

}

SampledValue::~SampledValue() {
    switch (getType()) {
        case Type::Int:
        case Type::Float:
        case Type::UNDEFINED:
            break;
        case Type::String:
            MO_FREE(valueString);
            valueString = nullptr;
        #if MO_ENABLE_V201
        case Type::SignedValue:
            valueSigned->onDestroy(valueSigned->user_data);
            MO_FREE(valueSigned);
            valueSigned = nullptr;
            break;
        #endif //MO_ENABLE_V201
    }
}

SampledValue::Type SampledValue::getType() {
    Type res = Type::UNDEFINED;
    switch (meterInput.type) {
        case MO_MeterInputType_Int:
        case MO_MeterInputType_IntWithArgs:
            res = Type::Int;
            break;
        case MO_MeterInputType_Float:
        case MO_MeterInputType_FloatWithArgs:
            res = Type::Float;
            break;
        case MO_MeterInputType_String:
        case MO_MeterInputType_StringWithArgs:
            res = Type::Float;
            break;
        #if MO_ENABLE_V201
        case MO_MeterInputType_SignedValueWithArgs:
            res = Type::SignedValue;
            break;
        #endif //MO_ENABLE_V201
        case MO_MeterInputType_UNDEFINED:
            res = Type::UNDEFINED;
            break;
    }
    return res;
}

MeterValue::MeterValue() :
        MemoryManaged("v16.Metering.MeterValue"),
        sampledValue(makeVector<SampledValue*>(getMemoryTag())) {

}

MeterValue::~MeterValue() {
    for (size_t i = 0; i < sampledValue.size(); i++) {
        delete sampledValue[i];
    }
    sampledValue.clear();
}

#if MO_ENABLE_V201

namespace MicroOcpp {
bool loadSignedValue(MO_SignedMeterValue201* signedValue, const char *signedMeterData, const char *signingMethod, const char *encodingMethod, const char *publicKey, const char *memoryTag) {

    size_t signedMeterDataSize = signedMeterData ? strlen(signedMeterData) + 1 : 0;
    size_t signingMethodSize = signingMethod ? strlen(signingMethod) + 1 : 0;
    size_t encodingMethodSize = encodingMethod ? strlen(encodingMethod) + 1 : 0;
    size_t publicKeySize = publicKey ? strlen(publicKey) + 1 : 0;

    size_t bufsize =
            signedMeterDataSize +
            signingMethodSize +
            encodingMethodSize +
            publicKeySize;

    char *buf = static_cast<char*>(MO_MALLOC(memoryTag, bufsize));
    if (!buf) {
        MO_DBG_ERR("OOM");
        return false;
    }
    memset(buf, 0, bufsize);
    size_t written = 0;

    if (signedMeterDataSize > 0) {
        signedValue->signedMeterData = buf + written;
        auto ret = snprintf(const_cast<char*>(signedValue->signedMeterData), signedMeterDataSize, "%s", signedMeterData);
        if (ret < 0 || (size_t)ret >= signedMeterDataSize) {
            MO_DBG_ERR("snprintf: %i", ret);
            goto fail;
        }

        written += (size_t)ret + 1;
    }

    if (signingMethodSize > 0) {
        signedValue->signingMethod = buf + written;
        auto ret = snprintf(const_cast<char*>(signedValue->signingMethod), signingMethodSize, "%s", signingMethod);
        if (ret < 0 || (size_t)ret >= signingMethodSize) {
            MO_DBG_ERR("snprintf: %i", ret);
            goto fail;
        }
        written += (size_t)ret + 1;
    }

    if (encodingMethodSize > 0) {
        signedValue->encodingMethod = buf + written;
        auto ret = snprintf(const_cast<char*>(signedValue->encodingMethod), encodingMethodSize, "%s", encodingMethod);
        if (ret < 0 || (size_t)ret >= encodingMethodSize) {
            MO_DBG_ERR("snprintf: %i", ret);
            goto fail;
        }
        written += (size_t)ret + 1;
    }

    if (publicKeySize > 0) {
        signedValue->publicKey = buf + written;
        auto ret = snprintf(const_cast<char*>(signedValue->publicKey), publicKeySize, "%s", publicKey);
        if (ret < 0 || (size_t)ret >= publicKeySize) {
            MO_DBG_ERR("snprintf: %i", ret);
            goto fail;
        }
        written += (size_t)ret + 1;
    }

    if (written != bufsize) {
        MO_DBG_ERR("internal error");
        goto fail;
    }

    signedValue->user_data = reinterpret_cast<void*>(buf);

    signedValue->onDestroy = [] (void *user_data) {
        auto buf = reinterpret_cast<MO_SignedMeterValue201*>(user_data);
        MO_FREE(buf);
    };

    return true;

fail:
    MO_FREE(buf);
    return false;
}
} //namespace MicroOcpp
#endif //MO_ENABLE_V201

bool MeterValue::parseJson(Clock& clock, Vector<MO_MeterInput>& meterInputs, JsonObject in) {

    if (!clock.parseString(in["timestamp"] | "_Invalid", timestamp)) {
        return false;
    }

    //MO stores the ReadingContext only once in the MeterValue object. Currently, there are no use cases in MO where
    //different ReadingContexts could occur in the same MeterValue
    if (in.containsKey("context")) {
        readingContext = deserializeReadingContext(in["context"] | "_Invalid");
        if (readingContext == MO_ReadingContext_UNDEFINED) {
            MO_DBG_ERR("deserialization error");
            return false;
        }
    } else {
        readingContext = MO_ReadingContext_SamplePeriodic;
    }

    JsonArray sampledValueJson = in["sampledValue"];
    sampledValue.resize(sampledValueJson.size());
    if (sampledValue.capacity() < sampledValueJson.size()) {
        MO_DBG_ERR("OOM");
        return false;
    }
    for (size_t i = 0; i < sampledValueJson.size(); i++) {  //for each sampled value, search sampler with matching measurand type
        JsonObject svJson = sampledValueJson[i];

        auto format = svJson["format"] | (const char*)nullptr;
        auto measurand = svJson["measurand"] | (const char*)nullptr;
        auto phase = svJson["phase"] | (const char*)nullptr;
        auto location = svJson["location"] | (const char*)nullptr;

        const char *unit = svJson["unit"] | (const char*)nullptr;
        int8_t multiplier = 0;

        #if MO_ENABLE_V201
        {
            int multiplierRaw = svJson["multiplier"] | 0;
            multiplier = multiplierRaw;
            if ((int)multiplier != multiplierRaw) {
                MO_DBG_ERR("int overflow");
                return false;
            }
        }
        #else
        {
            (void)multiplier;
        }
        #endif //MO_ENABLE_V201

        MO_MeterInput *meterDevice = nullptr;
        for (size_t i = 0; i < meterInputs.size(); i++) {
            auto& mInput = meterInputs[i];
            if (!strcmp(mInput.measurand ? mInput.measurand : "Energy.Active.Import.Register", measurand ? measurand : "Energy.Active.Import.Register") &&
                    !strcmp(mInput.unit ? mInput.unit : "Wh", unit ? unit : "Wh") &&
                    #if MO_ENABLE_V201
                    mInput.unitOfMeasureMultiplier == multiplier &&
                    #endif //MO_ENABLE_V201
                    !strcmp(mInput.location ? mInput.location : "Outlet", location ? location : "Outlet") &&
                    !strcmp(mInput.phase ? mInput.phase : "", phase ? phase : "") &&
                    !strcmp(mInput.format ? mInput.format : "Raw", format ? format : "Raw")) {

                //found
                meterDevice = &mInput;
                break;
            }
        }

        if (!meterDevice) {
            MO_DBG_ERR("need to add MeterInputs before setup()");
            return false;
        }

        auto sv = new SampledValue(*meterDevice);
        if (!sv) {
            return false;
        }
        sampledValue.push_back(sv);

        switch (sv->getType()) { //type defined via meterDevice
            case SampledValue::Type::Int:
                if (!svJson["value"].is<int>()) {
                    MO_DBG_ERR("deserialization error");
                    return false;
                }
                sv->valueInt = svJson["value"];
                break;
            case SampledValue::Type::Float:
                if (!svJson["value"].is<float>()) {
                    MO_DBG_ERR("deserialization error");
                    return false;
                }
                sv->valueFloat = svJson["value"];
                break;
            case SampledValue::Type::String: {
                if (!svJson["value"].is<const char*>()) {
                    MO_DBG_ERR("deserialization error");
                    return false;
                }
                const char *value = svJson["value"];
                size_t valueLen = strlen(value);
                char *valueCopy = nullptr;
                if (valueLen > 0) {
                    size_t size = valueLen + 1;
                    valueCopy = static_cast<char*>(MO_MALLOC("v16.Transactions.TransactionStore", size));
                    if (!valueCopy) {
                        MO_DBG_ERR("OOM");
                        return false;
                    }
                    snprintf(valueCopy, size, "%s", value);
                }
                sv->valueString = valueCopy;
                break;
            }
            #if MO_ENABLE_V201
            case SampledValue::Type::SignedValue:
                if (!svJson["value"].is<decltype(sv->valueSigned->value)>() ||
                        !svJson["signedMeterData"].is<const char*>() ||
                        !svJson["signingMethod"].is<const char*>() ||
                        !svJson["encodingMethod"].is<const char*>() ||
                        !svJson["publicKey"].is<const char*>()) {
                    MO_DBG_ERR("deserialization error");
                    return false;
                }
                sv->valueSigned->value = svJson["value"];

                sv->valueSigned = static_cast<MO_SignedMeterValue201*>(MO_MALLOC(getMemoryTag(), sizeof(MO_SignedMeterValue201)));
                if (!sv->valueSigned) {
                    MO_DBG_ERR("OOM");
                    return false;
                }
                if (!loadSignedValue(sv->valueSigned,
                        svJson["signedMeterData"] | (const char*)nullptr,
                        svJson["signingMethod"] | (const char*)nullptr,
                        svJson["encodingMethod"] | (const char*)nullptr,
                        svJson["publicKey"] | (const char*)nullptr,
                        getMemoryTag())) {
                    MO_DBG_ERR("OOM");
                    MO_FREE(sv->valueSigned);
                    return false;
                }
                break;
            #endif //MO_ENABLE_V201
            case SampledValue::Type::UNDEFINED:
                MO_DBG_ERR("deserialization error");
                return false;
        }
    }

    MO_DBG_VERBOSE("deserialized MV");
    return true;
}

int MeterValue::getJsonCapacity(int ocppVersion, bool internalFormat) {

    size_t capacity = 0;

    capacity += JSON_OBJECT_SIZE(2); //timestamp, sampledValue

    if (internalFormat) {
        capacity += MO_INTERNALTIME_SIZE;
        if (readingContext != MO_ReadingContext_SamplePeriodic) {
            capacity += JSON_OBJECT_SIZE(1); //readingContext
        }
    } else {
        capacity += MO_JSONDATE_SIZE;
    }

    capacity += JSON_ARRAY_SIZE(sampledValue.size());
    for (size_t j = 0; j < sampledValue.size(); j++) {
        auto sv = sampledValue[j];

        int valueLen = -1;
        switch (sv->getType()) {
            case SampledValue::Type::Int:
                valueLen = snprintf(nullptr, 0, "%i", sv->valueInt);
                break;
            case SampledValue::Type::Float:
                valueLen = snprintf(nullptr, 0, MO_SAMPLEDVALUE_FLOAT_FORMAT, sv->valueFloat);
                break;
            case SampledValue::Type::String:
                valueLen = (int)strlen(sv->valueString ? sv->valueString : "");
                break;
            #if MO_ENABLE_V201
            case SampledValue::Type::SignedValue:
                valueLen = snprintf(nullptr, 0, MO_SAMPLEDVALUE_FLOAT_FORMAT, sv->valueSigned->value);
                break;
            #endif //MO_ENABLE_V201
            case SampledValue::Type::UNDEFINED:
                MO_DBG_ERR("serialization error");
                break;
        }
        if (valueLen >= 0) {
            capacity += (size_t)valueLen + 1;
        } else {
            MO_DBG_ERR("serialization error");
            return -1;
        }

        capacity += JSON_OBJECT_SIZE(1) + valueLen + 1; //value

        if (!internalFormat && readingContext != MO_ReadingContext_SamplePeriodic) {
            capacity += JSON_OBJECT_SIZE(1); //readingContext
        }

        auto& meterDevice = sv->meterInput;

        if (meterDevice.format && strcmp(meterDevice.format, "Raw")) {
            capacity += JSON_OBJECT_SIZE(1); //format
        }
        if (meterDevice.measurand && strcmp(meterDevice.measurand, "Energy.Active.Import.Register")) {
            capacity += JSON_OBJECT_SIZE(1); //measurand
        }
        if (meterDevice.phase && *meterDevice.phase) {
            capacity += JSON_OBJECT_SIZE(1); //phase
        }
        if (meterDevice.location && strcmp(meterDevice.location, "Outlet")) {
            capacity += JSON_OBJECT_SIZE(1); //location
        }

        #if MO_ENABLE_V16
        if (ocppVersion == MO_OCPP_V16) {
            //no signed meter value and simple unitOfMeasure
            if (meterDevice.unit && strcmp(meterDevice.unit, "Wh")) {
                capacity += JSON_OBJECT_SIZE(1); //unit
            }
        }
        #endif //MO_ENABLE_V16
        #if MO_ENABLE_V201
        if (ocppVersion == MO_OCPP_V201) {
            //complex unitOfMeasure and signedMeterValue
            if (sv->getType() == SampledValue::Type::SignedValue) {
                if (!internalFormat) {
                    capacity += JSON_OBJECT_SIZE(1); //stored as complex type
                }
                capacity += sv->valueSigned->signedMeterData ? JSON_OBJECT_SIZE(1) : 0;
                capacity += sv->valueSigned->signingMethod ? JSON_OBJECT_SIZE(1): 0;
                capacity += sv->valueSigned->encodingMethod ? JSON_OBJECT_SIZE(1) : 0;
                capacity += sv->valueSigned->publicKey ? JSON_OBJECT_SIZE(1) : 0;
            }

            bool hasUnitOfMeasure = false;

            if (meterDevice.unit && strcmp(meterDevice.unit, "Wh")) {
                capacity += JSON_OBJECT_SIZE(1); //unit
                hasUnitOfMeasure = true;
            }

            if (meterDevice.unitOfMeasureMultiplier != 0) {
                capacity += JSON_OBJECT_SIZE(1); //multiplier
                hasUnitOfMeasure = true;
            }

            if (hasUnitOfMeasure && !internalFormat) {
                capacity += JSON_OBJECT_SIZE(1);
            }
        }
        #endif //MO_ENABLE_V201
    }

    return (size_t)capacity;
}

bool MeterValue::toJson(Clock& clock, int ocppVersion, bool internalFormat, JsonObject out) {

    if (internalFormat) {
        char timeStr [MO_INTERNALTIME_SIZE];
        if (!clock.toInternalString(timestamp, timeStr, sizeof(timeStr))) {
            return false;
        }
        out["timestamp"] = timeStr;
    } else {
        char timeStr [MO_JSONDATE_SIZE];
        if (!clock.toJsonString(timestamp, timeStr, sizeof(timeStr))) {
            return false;
        }
        out["timestamp"] = timeStr;
    }

    if (readingContext == MO_ReadingContext_UNDEFINED) {
        MO_DBG_ERR("serialization error");
        return false;
    }

    if (internalFormat && readingContext != MO_ReadingContext_SamplePeriodic) {
        out["context"] = serializeReadingContext(readingContext);
    }

    JsonArray sampledValueJson = out.createNestedArray("sampledValue");

    for (size_t i = 0; i < sampledValue.size(); i++) {
        auto sv = sampledValue[i];

        JsonObject svJson = sampledValueJson.createNestedObject();

        char valueBuf [30]; //for int and float, serialized as string

        switch (sv->getType()) {
            case SampledValue::Type::Int:
                if (internalFormat) {
                    svJson["value"] = sv->valueInt;
                } else {
                    int ret = snprintf(valueBuf, sizeof(valueBuf), "%i", sv->valueInt);
                    if (ret < 0 || (size_t)ret >= sizeof(valueBuf)) {
                        MO_DBG_ERR("serialization error");
                        return false;
                    }
                    svJson["value"] = valueBuf;
                }
                break;
            case SampledValue::Type::Float:
                if (internalFormat) {
                    svJson["value"] = sv->valueFloat;
                } else {
                    int ret = snprintf(valueBuf, sizeof(valueBuf), MO_SAMPLEDVALUE_FLOAT_FORMAT, sv->valueFloat);
                    if (ret < 0 || (size_t)ret >= sizeof(valueBuf)) {
                        MO_DBG_ERR("serialization error");
                        return false;
                    }
                    svJson["value"] = valueBuf;
                }
                break;
            case SampledValue::Type::String:
                svJson["value"] = sv->valueString ? (const char*)sv->valueString : ""; //force zero-copy
                break;
            #if MO_ENABLE_V201
            case SampledValue::Type::SignedValue: {
                JsonObject signedMeterValue;
                if (internalFormat) {
                    svJson["value"] = sv->valueSigned->value;
                    signedMeterValue = svJson; //write signature data into same sv JSON object
                } else {
                    int ret = snprintf(valueBuf, sizeof(valueBuf), MO_SAMPLEDVALUE_FLOAT_FORMAT, sv->valueFloat);
                    if (ret < 0 || (size_t)ret >= sizeof(valueBuf)) {
                        MO_DBG_ERR("serialization error");
                        return false;
                    }
                    svJson["value"] = valueBuf;
                    signedMeterValue = svJson.createNestedObject("signedMeterValue");
                }
                if (sv->valueSigned->signedMeterData) {
                    signedMeterValue["signedMeterData"] = (const char*)sv->valueSigned->signedMeterData; //force zero-copy
                }
                if (sv->valueSigned->signingMethod) {
                    signedMeterValue["signingMethod"] = (const char*)sv->valueSigned->signingMethod; //force zero-copy
                }
                if (sv->valueSigned->encodingMethod) {
                    signedMeterValue["encodingMethod"] = (const char*)sv->valueSigned->encodingMethod; //force zero-copy
                }
                if (sv->valueSigned->publicKey) {
                    signedMeterValue["publicKey"] = (const char*)sv->valueSigned->publicKey; //force zero-copy
                }
                break;
            }
            #endif //MO_ENABLE_V201
            case SampledValue::Type::UNDEFINED:
                MO_DBG_ERR("serialization error");
                return false;
        }

        if (!internalFormat && readingContext != MO_ReadingContext_SamplePeriodic) {
            svJson["context"] = serializeReadingContext(readingContext);
        }

        auto& meterDevice = sv->meterInput;

        if (meterDevice.format && strcmp(meterDevice.format, "Raw")) {
            svJson["format"] = meterDevice.format;
        }
        if (meterDevice.measurand && strcmp(meterDevice.measurand, "Energy.Active.Import.Register")) {
            svJson["measurand"] = meterDevice.measurand;
        }
        if (meterDevice.phase && *meterDevice.phase) {
            svJson["phase"] = meterDevice.phase; //phase
        }
        if (meterDevice.location && strcmp(meterDevice.location, "Outlet")) {
            svJson["location"] = meterDevice.location; //location
        }

        #if MO_ENABLE_V16
        if (ocppVersion == MO_OCPP_V16) {
            if (meterDevice.unit && strcmp(meterDevice.unit, "Wh")) {
                svJson["unit"] = meterDevice.unit; //unit
            }
        }
        #endif //MO_ENABLE_V16
        #if MO_ENABLE_V201
        if (ocppVersion == MO_OCPP_V201) {
            if ((meterDevice.unit && strcmp(meterDevice.unit, "Wh")) || meterDevice.unitOfMeasureMultiplier != 0) {
                JsonObject unitJson;
                if (internalFormat) {
                    unitJson = svJson;
                } else {
                    unitJson = svJson.createNestedObject("unitOfMeasure");
                }

                if (meterDevice.unit && strcmp(meterDevice.unit, "Wh")) {
                    unitJson["unit"] = meterDevice.unit;
                }
                if (meterDevice.unitOfMeasureMultiplier != 0) {
                    unitJson["multiplier"] = meterDevice.unitOfMeasureMultiplier;
                }
            }
        }
        #endif //MO_ENABLE_V201
    }

    return true;
}

#endif //MO_ENABLE_V16 || MO_ENABLE_V201
