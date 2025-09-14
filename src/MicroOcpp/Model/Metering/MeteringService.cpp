// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#include <cstddef>
#include <cinttypes>

#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Configuration/ConfigurationService.h>
#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Model/Metering/MeterStore.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlService.h>
#include <MicroOcpp/Model/Transactions/TransactionService16.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Operations/MeterValues.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16 || MO_ENABLE_V201

namespace MicroOcpp {

MeterValue *takeMeterValue(Clock& clock, Vector<MO_MeterInput>& meterInputs, unsigned int evseId, MO_ReadingContext readingContext, uint8_t inputSelectFlag, const char *memoryTag) {

    size_t samplesSize = 0;
    for (size_t i = 0; i < meterInputs.size(); i++) {
        if (meterInputs[i].mo_flags & inputSelectFlag) {
            samplesSize++;
        }
    }

    if (samplesSize == 0) {
        MO_DBG_DEBUG("no meter inputs selected");
        return nullptr;
    }

    auto meterValue = new MeterValue();
    if (!meterValue) {
        MO_DBG_ERR("OOM");
        goto fail;
    }

    meterValue->sampledValue.reserve(samplesSize);
    if (meterValue->sampledValue.capacity() < samplesSize) {
        MO_DBG_ERR("OOM");
        goto fail;
    }

    meterValue->timestamp = clock.now();
    meterValue->readingContext = readingContext;

    for (size_t i = 0; i < meterInputs.size(); i++) {
        auto& mInput = meterInputs[i];
        if (mInput.mo_flags & inputSelectFlag) {
            auto sv = new SampledValue(mInput);
            if (!sv) {
                MO_DBG_ERR("OOM");
                goto fail;
            }

            switch (mInput.type) {
                case MO_MeterInputType_Int:
                    sv->valueInt = mInput.getInt();
                    break;
                case MO_MeterInputType_IntWithArgs:
                    sv->valueInt = mInput.getInt2(readingContext, evseId, mInput.user_data);
                    break;
                case MO_MeterInputType_Float:
                    sv->valueFloat = mInput.getFloat();
                    break;
                case MO_MeterInputType_FloatWithArgs:
                    sv->valueFloat = mInput.getFloat2(readingContext, evseId, mInput.user_data);
                    break;
                case MO_MeterInputType_String:
                case MO_MeterInputType_StringWithArgs: {
                    char bufStack [100];
                    int ret = -1;
                    if (mInput.type == MO_MeterInputType_String) {
                        ret = mInput.getString(bufStack, sizeof(bufStack));
                    } else { //MO_MeterInputType_StringWithArgs
                        ret = mInput.getString2(bufStack, sizeof(bufStack), readingContext, evseId, mInput.user_data);
                    }
                    if (ret < 0 || ret > MO_SAMPLEDVALUE_STRING_MAX_LEN) {
                        MO_DBG_ERR("meterInput getString cb: %i", ret);
                        delete sv;
                        sv = nullptr;
                        break; //failure, skip this measurand
                    }

                    if (ret > 0) {
                        size_t size = ret + 1;
                        sv->valueString = static_cast<char*>(MO_MALLOC(memoryTag, size));
                        if (!sv->valueString) {
                            MO_DBG_ERR("OOM");
                            delete sv;
                            sv = nullptr;
                            break; //failure, skip this measurand
                        }
                        memset(sv->valueString, 0, size);

                        if ((size_t)ret < sizeof(bufStack)) {
                            //bufStack contains value
                            snprintf(sv->valueString, size, "%s", bufStack);
                        } else {
                            //need to re-run getString with properly sized buffer
                            if (mInput.type == MO_MeterInputType_String) {
                                ret = mInput.getString(sv->valueString, size);
                            } else { //MO_MeterInputType_StringWithArgs
                                ret = mInput.getString2(sv->valueString, size, readingContext, evseId, mInput.user_data);
                            }
                            if (ret < 0 || (size_t)ret >= size) {
                                MO_DBG_ERR("meterInput getString cb: %i", ret);
                                MO_FREE(sv->valueString);
                                sv->valueString = nullptr;
                                delete sv;
                                sv = nullptr;
                                break; //failure, skip this measurand
                            }
                        }
                    }

                    break; //success
                }
                #if MO_ENABLE_V201
                case MO_MeterInputType_SignedValueWithArgs:
                    sv->valueSigned = static_cast<MO_SignedMeterValue201*>(MO_MALLOC(memoryTag, sizeof(MO_SignedMeterValue201)));
                    if (!sv->valueSigned) {
                        MO_DBG_ERR("OOM");
                        delete sv;
                        sv = nullptr;
                        break; //failure, skip this measurand
                    }
                    memset(sv->valueSigned, 0, sizeof(*sv->valueSigned));

                    if (!mInput.getSignedValue2(sv->valueSigned, readingContext, evseId, mInput.user_data)) {
                        if (sv->valueSigned->onDestroy) {
                            sv->valueSigned->onDestroy(sv->valueSigned->user_data);
                        }
                        MO_FREE(sv->valueSigned);
                        delete sv;
                        sv = nullptr;
                        break; //undefined value skip this measurand
                    }
                    break;
                #endif //MO_ENABLE_V201
                case MO_MeterInputType_UNDEFINED:
                    MO_DBG_ERR("need to set type of meter input");
                    delete sv;
                    sv = nullptr;
                    break; //failure, skip this measurand
            }

            if (sv) {
                meterValue->sampledValue.push_back(sv);
            }
        }
    }

    return meterValue;
fail:
    delete meterValue;
    meterValue = nullptr;
    return nullptr;
}

void updateInputSelectFlag(const char *selectString, uint8_t flag, Vector<MO_MeterInput>& meterInputs) {
    auto len = strlen(selectString);
    size_t sl = 0, sr = 0;
    while (selectString && sl < len) {
        while (sr < len) {
            if (selectString[sr] == ',') {
                break;
            }
            sr++;
        }

        if (sr != sl + 1) {
            const char *csvMeasurand = selectString + sl;
            size_t csvMeasurandLen = sr - sl;

            for (size_t i = 0; i < meterInputs.size(); i++) {
                const char *measurand = meterInputs[i].measurand ? meterInputs[i].measurand : "Energy.Active.Import.Register";
                if (!strncmp(measurand, csvMeasurand, csvMeasurandLen) && strlen(measurand) == csvMeasurandLen) {
                    meterInputs[i].mo_flags |= flag;
                } else {
                    meterInputs[i].mo_flags &= ~flag;
                }
            }
        }

        sr++;
        sl = sr;
    }
}

bool validateSelectString(const char *selectString, void *user_data) {
    auto& context = *reinterpret_cast<Context*>(user_data);

    bool isValid = true;
    const char *l = selectString; //the beginning of an entry of the comma-separated list
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

        const char *csvMeasurand = l;
        size_t csvMeasurandLen = (size_t)(r - l);

        //check if measurand exists in MeterInputs. Search through all EVSEs
        bool found = false;
        for (unsigned int evseId = 0; evseId < context.getModelCommon().getNumEvseId(); evseId++) {

            Vector<MO_MeterInput> *meterInputsPtr = nullptr;

            #if MO_ENABLE_V16
            if (context.getOcppVersion() == MO_OCPP_V16) {
                auto mService = context.getModel16().getMeteringService();
                auto evse = mService ? mService->getEvse(evseId) : nullptr;
                meterInputsPtr = evse ? &evse->getMeterInputs() : nullptr;
            }
            #endif //MO_ENABLE_V16
            #if MO_ENABLE_V201
            if (context.getOcppVersion() == MO_OCPP_V201) {
                auto mService = context.getModel201().getMeteringService();
                auto evse = mService ? mService->getEvse(evseId) : nullptr;
                meterInputsPtr = evse ? &evse->getMeterInputs() : nullptr;
            }
            #endif //MO_ENABLE_V201

            if (!meterInputsPtr) {
                MO_DBG_ERR("internal error");
                continue;
            }

            auto& meterInputs = *meterInputsPtr;

            for (size_t i = 0; i < meterInputs.size(); i++) {
                const char *measurand = meterInputs[i].measurand ? meterInputs[i].measurand : "Energy.Active.Import.Register";
                if (!strncmp(measurand, csvMeasurand, csvMeasurandLen) && strlen(measurand) == csvMeasurandLen) {
                    found = true;
                    break;
                }
            }

            if (found) {
                break;
            }
        }
        if (!found) {
            isValid = false;
            MO_DBG_WARN("could not find metering device for %.*s", (int)csvMeasurandLen, csvMeasurand);
            break;
        }
        l = r;
    }
    return isValid;
}

} //namespace MicroOcpp

#endif //MO_ENABLE_V16 || MO_ENABLE_V201

#if MO_ENABLE_V16

#define MO_FLAG_MeterValuesSampledData (1 << 0)
#define MO_FLAG_MeterValuesAlignedData (1 << 1)
#define MO_FLAG_StopTxnSampledData     (1 << 2)
#define MO_FLAG_StopTxnAlignedData     (1 << 3)

using namespace MicroOcpp;

v16::MeteringServiceEvse::MeteringServiceEvse(Context& context, MeteringService& mService, unsigned int connectorId)
        : MemoryManaged("v16.Metering.MeteringServiceEvse"), context(context), clock(context.getClock()), model(context.getModel16()), mService(mService), connectorId(connectorId), meterData(makeVector<MeterValue*>(getMemoryTag())), meterInputs(makeVector<MO_MeterInput>(getMemoryTag())) {

}

v16::MeteringServiceEvse::~MeteringServiceEvse() {
    for (size_t i = 0; i < meterData.size(); i++) {
        delete meterData[i];
    }
    meterData.clear();
    delete meterDataFront;
    meterDataFront = nullptr;
}

bool v16::MeteringServiceEvse::addMeterInput(MO_MeterInput meterInput) {
    auto capacity = meterInputs.size() + 1;
    meterInputs.reserve(capacity);
    if (meterInputs.capacity() < capacity) {
        MO_DBG_ERR("OOM");
        return false;
    }

    meterInput.mo_flags = 0;
    meterInputs.push_back(meterInput);
    return true;
}

Vector<MO_MeterInput>& v16::MeteringServiceEvse::getMeterInputs() {
    return meterInputs;
}

bool v16::MeteringServiceEvse::setTxEnergyMeterInput(int32_t (*getInt)()) {
    txEnergyMeterInput = getInt;
    return true;
}

bool v16::MeteringServiceEvse::setTxEnergyMeterInput2(int32_t (*getInt2)(MO_ReadingContext readingContext, unsigned int evseId, void *user_data), void *user_data) {
    txEnergyMeterInput2 = getInt2;
    txEnergyMeterInput2_user_data = user_data;
    return true;
}

bool v16::MeteringServiceEvse::setup() {

    context.getMessageService().addSendQueue(this);

    lastAlignedTime = context.getClock().now();

    filesystem = context.getFilesystem();
    if (!filesystem) {
        MO_DBG_DEBUG("volatile mode");
    }

    auto txSvc = context.getModel16().getTransactionService();
    txSvcEvse = txSvc ? txSvc->getEvse(connectorId) : nullptr;
    if (!txSvcEvse) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    auto configService = context.getModel16().getConfigurationService();
    if (!configService) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    if (!txEnergyMeterInput && !txEnergyMeterInput2) {
        //search default energy meter
        bool found = false;
        for (size_t i = 0; i < meterInputs.size(); i++) {
            auto& mInput = meterInputs[i];
            if (mInput.measurand && !strcmp(mInput.measurand, "Energy.Active.Import.Register") &&
                    (!mInput.phase || !*mInput.phase) &&
                    (!mInput.location || !strcmp(mInput.location, "Outlet")) &&
                    (!mInput.unit || !strcmp(mInput.unit, "Wh")) &&
                    (mInput.type == MO_MeterInputType_Int || mInput.type == MO_MeterInputType_IntWithArgs)) {

                if (mInput.type == MO_MeterInputType_Int) {
                    txEnergyMeterInput = mInput.getInt;
                } else {
                    txEnergyMeterInput2 = mInput.getInt2;
                    txEnergyMeterInput2_user_data = mInput.user_data;
                }
                found = true;
            }
        }

        if (found) {
            MO_DBG_DEBUG("found energy input for Start- and StopTx values");
        } else {
            MO_DBG_WARN("no energy meter set up. Start- and StopTx will report 0 Wh");
        }
    }

    return true;
}

MicroOcpp::MeterValue *v16::MeteringServiceEvse::takeMeterValue(MO_ReadingContext readingContext, uint8_t inputSelectFlag) {
    return MicroOcpp::takeMeterValue(context.getClock(), meterInputs, connectorId, readingContext, inputSelectFlag, getMemoryTag());
}

void v16::MeteringServiceEvse::loop() {

    bool txBreak = false;
    auto transaction = txSvcEvse->getTransaction();
    txBreak = (transaction && transaction->isRunning()) != trackTxRunning;
    trackTxRunning = (transaction && transaction->isRunning());

    if (txBreak) {
        lastSampleTime = clock.getUptimeInt();
    }

    if (!transaction && connectorId != 0 && mService.meterValuesInTxOnlyBool->getBool()) {
        //don't take any MeterValues outside of transactions on connectorIds other than 0
        return;
    }

    updateInputSelectFlags();

    if (mService.clockAlignedDataIntervalInt->getInt() >= 1 && clock.now().isUnixTime()) {

        bool shouldTakeMeasurement = false;
        bool shouldUpdateLastTime = false;

        auto& timestampNow = clock.now();
        int32_t dt;
        bool dt_defined = clock.delta(timestampNow, lastAlignedTime, dt);
        if (!dt_defined) {
            //lastAlignedTime not set yet. Do that now
            shouldUpdateLastTime = true;
            MO_DBG_DEBUG("Clock aligned measurement: initial setting");
        } else if (dt < 0) {
            //timestampNow before lastAligned time - probably the clock has been adjusted
            shouldUpdateLastTime = true;
            MO_DBG_DEBUG("Clock aligned measurement: correct time after clock adjustment (%i)", (int)dt);
        } else if (dt < mService.clockAlignedDataIntervalInt->getInt()) {
            //dt is number of seconds elapsed since last measurements and seconds < interval, so do nothing
            (void)0;
        } else if (dt <= mService.clockAlignedDataIntervalInt->getInt() + 60) {
            //elapsed seconds >= interval and still within tolerance band of 60 seconds, take measurement!
            shouldTakeMeasurement = true;
            shouldUpdateLastTime = true;
            MO_DBG_DEBUG("Clock aligned measurement: take measurement (%i)", (int)dt);
        } else {
            //elapsed seconds is past the tolerance band. Measurement wouldn't be clock-aligned anymore, so skip
            shouldUpdateLastTime = true;
            MO_DBG_DEBUG("Clock aligned measurement: tolerance exceeded, skip measurement (%i)", (int)dt);
        }

        if (shouldTakeMeasurement) {

            if (auto alignedMeterValue = takeMeterValue(MO_ReadingContext_SampleClock, MO_FLAG_MeterValuesAlignedData)) {
                if (meterData.size() >= MO_MVRECORD_SIZE) {
                    MO_DBG_INFO("MeterValue cache full. Drop old MV");
                    auto mv = meterData.begin();
                    delete *mv;
                    meterData.erase(mv);
                }
                alignedMeterValue->opNr = context.getMessageService().getNextOpNr();
                if (transaction) {
                    alignedMeterValue->txNr = transaction->getTxNr();
                }
                meterData.push_back(alignedMeterValue);
            }

            if (transaction) {
                if (auto alignedStopTx = takeMeterValue(MO_ReadingContext_SampleClock, MO_FLAG_StopTxnAlignedData)) {
                    addTxMeterData(*transaction, alignedStopTx);
                }
            }
        }

        if (shouldUpdateLastTime) {
            Timestamp midnightBase;
            clock.parseString("2010-01-01T00:00:00Z", midnightBase);
            int32_t dt;
            clock.delta(timestampNow, midnightBase, dt); //dt is the number of seconds since Jan 1 2010
            dt %= (int32_t)(3600 * 24); //dt is the number of seconds since midnight
            Timestamp midnight = timestampNow;
            clock.add(midnight, -dt); //midnight is last midnight (i.e. today at 00:00)
            if (dt < mService.clockAlignedDataIntervalInt->getInt()) {
                //close to midnight, so align to midnight
                lastAlignedTime = midnight;
            } else {
                //at least one period since midnight has elapsed, so align to last period
                dt /= mService.clockAlignedDataIntervalInt->getInt(); //floor dt to the last full period
                dt *= mService.clockAlignedDataIntervalInt->getInt();
                lastAlignedTime = midnight;
                clock.add(lastAlignedTime, dt);
            }
        }
    }

    if (mService.meterValueSampleIntervalInt->getInt() >= 1) {
        //record periodic tx data

        if (clock.getUptimeInt() - lastSampleTime >= mService.meterValueSampleIntervalInt->getInt()) {
            if (auto sampledMeterValue = takeMeterValue(MO_ReadingContext_SamplePeriodic, MO_FLAG_MeterValuesSampledData)) {
                if (meterData.size() >= MO_MVRECORD_SIZE) {
                    auto mv = meterData.begin();
                    delete *mv;
                    meterData.erase(mv);
                }
                sampledMeterValue->opNr = context.getMessageService().getNextOpNr();
                if (transaction) {
                    sampledMeterValue->txNr = transaction->getTxNr();
                }
                meterData.push_back(sampledMeterValue);
            }

            if (transaction && mService.stopTxnDataCapturePeriodicBool->getBool()) {
                if (auto sampleStopTx = takeMeterValue(MO_ReadingContext_SamplePeriodic, MO_FLAG_StopTxnSampledData)) {
                    addTxMeterData(*transaction, sampleStopTx);
                }
            }

            lastSampleTime = clock.getUptimeInt();
        }
    }
}

Operation *v16::MeteringServiceEvse::createTriggeredMeterValues() {

    auto meterValue = takeMeterValue(MO_ReadingContext_Trigger, MO_FLAG_MeterValuesSampledData);
    if (!meterValue) {
        MO_DBG_ERR("OOM");
        return nullptr;
    }

    int transactionId = -1;
    if (auto tx = txSvcEvse->getTransaction()) {
        transactionId = tx->getTransactionId();
    }

    auto operation = new MeterValues(context, connectorId, transactionId, meterValue, /*transferOwnership*/ true);
    if (!operation) {
        MO_DBG_ERR("OOM");
        delete meterValue;
        return nullptr;
    }

    return operation;
}

int32_t v16::MeteringServiceEvse::readTxEnergyMeter(MO_ReadingContext readingContext) {
    if (txEnergyMeterInput) {
        return txEnergyMeterInput();
    } else if (txEnergyMeterInput2) {
        return txEnergyMeterInput2(readingContext, connectorId, txEnergyMeterInput2_user_data);
    } else {
        return 0;
    }
}

bool v16::MeteringServiceEvse::addTxMeterData(Transaction& transaction, MeterValue *mv) {

    auto& txMeterValues = transaction.getTxMeterValues();

    unsigned int mvIndex;
    bool replaceEntry = false;
    if (txMeterValues.size() >= MO_STOPTXDATA_MAX_SIZE) {
        //replace last entry
        mvIndex = txMeterValues.size() - 1;
        replaceEntry = true;
    } else {
        //append
        mvIndex = txMeterValues.size();
    }

    size_t capacity = mvIndex + 1;
    txMeterValues.resize(capacity);
    if (txMeterValues.size() < capacity) {
        MO_DBG_ERR("OOM");
        goto fail;
    }

    if (filesystem) {
        auto ret = MeterStore::store(filesystem, context, connectorId, transaction.getTxNr(), mvIndex, *mv);
        if (ret != FilesystemUtils::StoreStatus::Success) {
            MO_DBG_ERR("fs failure");
            goto fail;
        }
    }

    if (replaceEntry) {
        delete txMeterValues.back();
        txMeterValues.back() = mv;
        MO_DBG_DEBUG("updated latest sd");
    } else {
        txMeterValues.emplace_back(mv);
        MO_DBG_DEBUG("added sd");
    }
    return true;
fail:
    delete mv;
    return false;
}

bool v16::MeteringServiceEvse::beginTxMeterData(Transaction *transaction) {

    auto& txMeterValues = transaction->getTxMeterValues();

    if (!txMeterValues.empty()) {
        //first MV already taken
        return true;
    }

    auto sampleTxBegin = takeMeterValue(MO_ReadingContext_TransactionBegin, MO_FLAG_StopTxnSampledData);
    if (!sampleTxBegin) {
        return true;
    }

    return addTxMeterData(*transaction, sampleTxBegin);
}

bool v16::MeteringServiceEvse::endTxMeterData(Transaction *transaction) {

    auto& txMeterValues = transaction->getTxMeterValues();

    if (!txMeterValues.empty() && txMeterValues.back()->readingContext == MO_ReadingContext_TransactionEnd) {
        //final MV already taken
        return true;
    }

    auto sampleTxEnd = takeMeterValue(MO_ReadingContext_TransactionEnd, MO_FLAG_StopTxnSampledData);
    if (!sampleTxEnd) {
        return true;
    }

    return addTxMeterData(*transaction, sampleTxEnd);
}

void v16::MeteringServiceEvse::updateInputSelectFlags() {
    uint16_t selectInputsWriteCount = 0;
    selectInputsWriteCount += mService.meterValuesSampledDataString->getWriteCount();
    selectInputsWriteCount += mService.stopTxnSampledDataString->getWriteCount();
    selectInputsWriteCount += mService.meterValuesAlignedDataString->getWriteCount();
    selectInputsWriteCount += mService.stopTxnAlignedDataString->getWriteCount();
    selectInputsWriteCount += (uint16_t)meterInputs.size(); //also invalidate if new meterInputs were added
    if (selectInputsWriteCount != trackSelectInputs) {
        MO_DBG_DEBUG("Updating observed samplers after config change or meterInputs added");
        updateInputSelectFlag(mService.meterValuesSampledDataString->getString(), MO_FLAG_MeterValuesSampledData, meterInputs);
        updateInputSelectFlag(mService.stopTxnSampledDataString->getString(), MO_FLAG_StopTxnSampledData, meterInputs);
        updateInputSelectFlag(mService.meterValuesAlignedDataString->getString(), MO_FLAG_MeterValuesAlignedData, meterInputs);
        updateInputSelectFlag(mService.stopTxnAlignedDataString->getString(), MO_FLAG_StopTxnAlignedData, meterInputs);
        trackSelectInputs = selectInputsWriteCount;
    }
}

unsigned int v16::MeteringServiceEvse::getFrontRequestOpNr() {
    if (!meterDataFront && !meterData.empty()) {
        MO_DBG_DEBUG("advance MV front");
        meterDataFront = meterData.front(); //transfer ownership
        meterData.erase(meterData.begin());
    }
    if (meterDataFront) {
        return meterDataFront->opNr;
    }
    return NoOperation;
}

std::unique_ptr<Request> v16::MeteringServiceEvse::fetchFrontRequest() {

    if (!mService.connection->isConnected()) {
        //offline behavior: pause sending messages and do not increment attempt counters
        return nullptr;
    }

    if (!meterDataFront) {
        return nullptr;
    }

    if (meterValuesInProgress) {
        //ensure that only one MeterValues request is being executed at the same time
        return nullptr;
    }

    if ((int)meterDataFront->attemptNr >= mService.transactionMessageAttemptsInt->getInt()) {
        MO_DBG_WARN("exceeded TransactionMessageAttempts. Discard MeterValue");
        delete meterDataFront;
        meterDataFront = nullptr;
        return nullptr;
    }

    int32_t dtLastAttempt = MO_MAX_TIME;
    if (meterDataFront->attemptTime.isDefined() &&
            !clock.delta(clock.getUptime(), meterDataFront->attemptTime, dtLastAttempt)) {
        MO_DBG_ERR("internal error");
    }

    if (dtLastAttempt < (int)meterDataFront->attemptNr * std::max(0, mService.transactionMessageRetryIntervalInt->getInt())) {
        return nullptr;
    }

    meterDataFront->attemptNr++;
    meterDataFront->attemptTime = clock.getUptime();

    auto transaction = txSvcEvse->getTransactionFront();
    bool txNrMatch = (!transaction && meterDataFront->txNr < 0) || (transaction && meterDataFront->txNr >= 0 &&  (unsigned int)meterDataFront->txNr == transaction->getTxNr());
    if (!txNrMatch) {
        MO_DBG_ERR("txNr mismatch"); //this could happen if tx-related MeterValues are sent shortly before StartTx or shortly after StopTx, or if txNr is mis-assigned during creation
        transaction = nullptr;
    }

    //discard MV if it belongs to silent tx
    if (transaction && transaction->isSilent()) {
        MO_DBG_DEBUG("Drop MeterValue belonging to silent tx");
        delete meterDataFront;
        meterDataFront = nullptr;
        return nullptr;
    }

    int transactionId = -1;
    if (transaction) {
        transactionId = transaction->getTransactionId();
    }

    auto meterValues = makeRequest(context, new MeterValues(context, connectorId, transactionId, meterDataFront, /*transferOwnership*/ false));
    meterValues->setOnReceiveConf([this] (JsonObject) {
        meterValuesInProgress = false;

        //operation success
        MO_DBG_DEBUG("drop MV front");
        delete meterDataFront;
        meterDataFront = nullptr;
    });
    meterValues->setOnAbort([this] () {
        meterValuesInProgress = false;
    });

    meterValuesInProgress = true;

    return meterValues;
}

v16::MeteringService::MeteringService(Context& context)
      : MemoryManaged("v16.Metering.MeteringService"), context(context) {

}

v16::MeteringService::~MeteringService() {
    for (unsigned int i = 0; i < MO_NUM_EVSEID; i++) {
        delete evses[i];
        evses[i] = nullptr;
    }
}

bool v16::MeteringService::setup() {

    connection = context.getConnection();
    if (!connection) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    auto configService = context.getModel16().getConfigurationService();
    if (!configService) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    meterValuesSampledDataString = configService->declareConfiguration<const char*>("MeterValuesSampledData", "Energy.Active.Import.Register,Power.Active.Import");
    stopTxnSampledDataString = configService->declareConfiguration<const char*>("StopTxnSampledData", "");
    meterValueSampleIntervalInt = configService->declareConfiguration<int>("MeterValueSampleInterval", 60);

    meterValuesAlignedDataString = configService->declareConfiguration<const char*>("MeterValuesAlignedData", "");
    stopTxnAlignedDataString = configService->declareConfiguration<const char*>("StopTxnAlignedData", "");
    clockAlignedDataIntervalInt  = configService->declareConfiguration<int>("ClockAlignedDataInterval", 0);

    meterValuesInTxOnlyBool = configService->declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "MeterValuesInTxOnly", true);
    stopTxnDataCapturePeriodicBool = configService->declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "StopTxnDataCapturePeriodic", false);

    transactionMessageAttemptsInt = configService->declareConfiguration<int>("TransactionMessageAttempts", 3);
    transactionMessageRetryIntervalInt = configService->declareConfiguration<int>("TransactionMessageRetryInterval", 60);

    if (!meterValuesSampledDataString ||
            !stopTxnSampledDataString ||
            !meterValueSampleIntervalInt ||
            !meterValuesAlignedDataString ||
            !stopTxnAlignedDataString ||
            !clockAlignedDataIntervalInt ||
            !meterValuesInTxOnlyBool ||
            !stopTxnDataCapturePeriodicBool ||
            !transactionMessageAttemptsInt ||
            !transactionMessageRetryIntervalInt) {
        MO_DBG_ERR("declareConfiguration failed");
        return false;
    }

    configService->registerValidator<const char*>("MeterValuesSampledData", validateSelectString, reinterpret_cast<void*>(&context));
    configService->registerValidator<const char*>("StopTxnSampledData", validateSelectString, reinterpret_cast<void*>(&context));
    configService->registerValidator<int>("MeterValueSampleInterval", VALIDATE_UNSIGNED_INT);

    configService->registerValidator<const char*>("MeterValuesAlignedData", validateSelectString, reinterpret_cast<void*>(&context));
    configService->registerValidator<const char*>("StopTxnAlignedData", validateSelectString, reinterpret_cast<void*>(&context));
    configService->registerValidator<int>("ClockAlignedDataInterval", VALIDATE_UNSIGNED_INT);

    auto rcService = context.getModel16().getRemoteControlService();
    if (!rcService) {
        MO_DBG_ERR("initialization error");
        return false;
    }

    rcService->addTriggerMessageHandler("MeterValues", [] (Context& context, unsigned int evseId) {
        auto mvSvc = context.getModel16().getMeteringService();
        auto mvSvcEvse = mvSvc ? mvSvc->getEvse(evseId) : nullptr;
        return mvSvcEvse ? mvSvcEvse->createTriggeredMeterValues() : nullptr;
    });

    #if MO_ENABLE_MOCK_SERVER
    context.getMessageService().registerOperation("MeterValues", nullptr, nullptr);
    #endif //MO_ENABLE_MOCK_SERVER

    numEvseId = context.getModel16().getNumEvseId();
    for (unsigned int i = 0; i < numEvseId; i++) {
        if (!getEvse(i) || !getEvse(i)->setup()) {
            MO_DBG_ERR("connector init failure");
            return false;
        }
    }

    return true;
}

void v16::MeteringService::loop(){
    for (unsigned int i = 0; i < numEvseId; i++){
        evses[i]->loop();
    }
}

v16::MeteringServiceEvse *v16::MeteringService::getEvse(unsigned int evseId) {
    if (evseId >= numEvseId) {
        MO_DBG_ERR("evseId out of bound");
        return nullptr;
    }

    if (!evses[evseId]) {
        evses[evseId] = new MeteringServiceEvse(context, *this, evseId);
        if (!evses[evseId]) {
            MO_DBG_ERR("OOM");
            return nullptr;
        }
    }

    return evses[evseId];
}

#endif //MO_ENABLE_V16

#if MO_ENABLE_V201

#define MO_FLAG_SampledDataTxStarted (1 << 0)
#define MO_FLAG_SampledDataTxUpdated (1 << 1)
#define MO_FLAG_SampledDataTxEnded   (1 << 2)
#define MO_FLAG_AlignedData          (1 << 3)

using namespace MicroOcpp;

v201::MeteringServiceEvse::MeteringServiceEvse(Context& context, MeteringService& mService, unsigned int evseId)
        : MemoryManaged("v201.MeterValues.MeteringServiceEvse"), context(context), mService(mService), evseId(evseId), meterInputs(makeVector<MO_MeterInput>(getMemoryTag())) {

}

bool v201::MeteringServiceEvse::setup() {
    return true; //nothing to be done here
}

bool v201::MeteringServiceEvse::addMeterInput(MO_MeterInput meterInput) {
    auto capacity = meterInputs.size() + 1;
    meterInputs.reserve(capacity);
    if (meterInputs.capacity() < capacity) {
        MO_DBG_ERR("OOM");
        return false;
    }

    meterInput.mo_flags = 0;
    meterInputs.push_back(meterInput);
    return true;
}

Vector<MO_MeterInput>& v201::MeteringServiceEvse::getMeterInputs() {
    return meterInputs;
}

void v201::MeteringServiceEvse::updateInputSelectFlags() {
    uint16_t selectInputsWriteCount = 0;
    selectInputsWriteCount += mService.sampledDataTxStartedMeasurands->getWriteCount();
    selectInputsWriteCount += mService.sampledDataTxUpdatedMeasurands->getWriteCount();
    selectInputsWriteCount += mService.sampledDataTxEndedMeasurands->getWriteCount();
    selectInputsWriteCount += mService.alignedDataMeasurands->getWriteCount();
    selectInputsWriteCount += (uint16_t)meterInputs.size(); //also invalidate if new meterInputs were added
    if (selectInputsWriteCount != trackSelectInputs) {
        MO_DBG_DEBUG("Updating observed samplers after variables change or meterInputs added");
        updateInputSelectFlag(mService.sampledDataTxStartedMeasurands->getString(), MO_FLAG_SampledDataTxStarted, meterInputs);
        updateInputSelectFlag(mService.sampledDataTxUpdatedMeasurands->getString(), MO_FLAG_SampledDataTxUpdated, meterInputs);
        updateInputSelectFlag(mService.sampledDataTxEndedMeasurands->getString(), MO_FLAG_SampledDataTxEnded, meterInputs);
        updateInputSelectFlag(mService.alignedDataMeasurands->getString(), MO_FLAG_AlignedData, meterInputs);
        trackSelectInputs = selectInputsWriteCount;
    }
}

std::unique_ptr<MeterValue> v201::MeteringServiceEvse::takeTxStartedMeterValue(MO_ReadingContext readingContext) {
    updateInputSelectFlags();
    return std::unique_ptr<MeterValue>(MicroOcpp::takeMeterValue(context.getClock(), meterInputs, evseId, readingContext, MO_FLAG_SampledDataTxStarted, getMemoryTag()));
}
std::unique_ptr<MeterValue> v201::MeteringServiceEvse::takeTxUpdatedMeterValue(MO_ReadingContext readingContext) {
    updateInputSelectFlags();
    return std::unique_ptr<MeterValue>(MicroOcpp::takeMeterValue(context.getClock(), meterInputs, evseId, readingContext, MO_FLAG_SampledDataTxUpdated, getMemoryTag()));
}
std::unique_ptr<MeterValue> v201::MeteringServiceEvse::takeTxEndedMeterValue(MO_ReadingContext readingContext) {
    updateInputSelectFlags();
    return std::unique_ptr<MeterValue>(MicroOcpp::takeMeterValue(context.getClock(), meterInputs, evseId, readingContext, MO_FLAG_SampledDataTxEnded, getMemoryTag()));
}
std::unique_ptr<MeterValue> v201::MeteringServiceEvse::takeTriggeredMeterValues() {
    updateInputSelectFlags();
    return std::unique_ptr<MeterValue>(MicroOcpp::takeMeterValue(context.getClock(), meterInputs, evseId, MO_ReadingContext_Trigger, MO_FLAG_AlignedData, getMemoryTag()));
}

v201::MeteringService::MeteringService(Context& context) : MemoryManaged("v201.Metering.MeteringService"), context(context) {

}

v201::MeteringService::~MeteringService() {
    for (size_t i = 0; i < MO_NUM_EVSEID; i++) {
        delete evses[i];
        evses[i] = nullptr;
    }
}

v201::MeteringServiceEvse *v201::MeteringService::getEvse(unsigned int evseId) {
    if (evseId >= numEvseId) {
        MO_DBG_ERR("evseId out of bound");
        return nullptr;
    }

    if (!evses[evseId]) {
        evses[evseId] = new MeteringServiceEvse(context, *this, evseId);
        if (!evses[evseId]) {
            MO_DBG_ERR("OOM");
            return nullptr;
        }
    }

    return evses[evseId];
}

bool v201::MeteringService::setup() {

    auto varService = context.getModel201().getVariableService();
    if (!varService) {
        return false;
    }

    sampledDataTxStartedMeasurands = varService->declareVariable<const char*>("SampledDataCtrlr", "TxStartedMeasurands", "");
    sampledDataTxUpdatedMeasurands = varService->declareVariable<const char*>("SampledDataCtrlr", "TxUpdatedMeasurands", "");
    sampledDataTxEndedMeasurands = varService->declareVariable<const char*>("SampledDataCtrlr", "TxEndedMeasurands", "");
    alignedDataMeasurands = varService->declareVariable<const char*>("AlignedDataCtrlr", "AlignedDataMeasurands", "");

    if (!sampledDataTxStartedMeasurands ||
            !sampledDataTxUpdatedMeasurands ||
            !sampledDataTxEndedMeasurands ||
            !alignedDataMeasurands) {
        MO_DBG_ERR("failure to declare variable");
        return false;
    }

    //define factory defaults
    varService->declareVariable<const char*>("SampledDataCtrlr", "TxStartedMeasurands", "");
    varService->declareVariable<const char*>("SampledDataCtrlr", "TxUpdatedMeasurands", "");
    varService->declareVariable<const char*>("SampledDataCtrlr", "TxEndedMeasurands", "");
    varService->declareVariable<const char*>("AlignedDataCtrlr", "AlignedDataMeasurands", "");

    varService->registerValidator<const char*>("SampledDataCtrlr", "TxStartedMeasurands", validateSelectString, reinterpret_cast<void*>(&context));
    varService->registerValidator<const char*>("SampledDataCtrlr", "TxUpdatedMeasurands", validateSelectString, reinterpret_cast<void*>(&context));
    varService->registerValidator<const char*>("SampledDataCtrlr", "TxEndedMeasurands", validateSelectString, reinterpret_cast<void*>(&context));
    varService->registerValidator<const char*>("AlignedDataCtrlr", "AlignedDataMeasurands", validateSelectString, reinterpret_cast<void*>(&context));

    numEvseId = context.getModel201().getNumEvseId();
    for (size_t i = 0; i < numEvseId; i++) {
        if (!getEvse(i) || !getEvse(i)->setup()) {
            MO_DBG_ERR("setup failure");
            return false;
        }
    }

    return true;
}

#endif //MO_ENABLE_V201
