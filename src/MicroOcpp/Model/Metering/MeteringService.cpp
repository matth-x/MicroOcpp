// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#include <cstddef>
#include <cinttypes>

#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Operations/MeterValues.h>
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;
using namespace MicroOcpp::Ocpp16;

MeteringServiceEvse::MeteringServiceEvse(Context& context, int connectorId, MeterStore& meterStore)
        : MemoryManaged("v16.Metering.MeteringServiceEvse"), context(context), model(context.getModel()), connectorId{connectorId}, meterStore(meterStore), meterData(makeVector<std::unique_ptr<MeterValue>>(getMemoryTag())), samplers(makeVector<std::unique_ptr<SampledValueSampler>>(getMemoryTag())) {

    context.getRequestQueue().addSendQueue(this);

    auto meterValuesSampledDataString = declareConfiguration<const char*>("MeterValuesSampledData", "");
    declareConfiguration<int>("MeterValuesSampledDataMaxLength", 8, CONFIGURATION_VOLATILE, true);
    meterValueSampleIntervalInt = declareConfiguration<int>("MeterValueSampleInterval", 60);
    registerConfigurationValidator("MeterValueSampleInterval", VALIDATE_UNSIGNED_INT);

    auto stopTxnSampledDataString = declareConfiguration<const char*>("StopTxnSampledData", "");
    declareConfiguration<int>("StopTxnSampledDataMaxLength", 8, CONFIGURATION_VOLATILE, true);

    auto meterValuesAlignedDataString = declareConfiguration<const char*>("MeterValuesAlignedData", "");
    declareConfiguration<int>("MeterValuesAlignedDataMaxLength", 8, CONFIGURATION_VOLATILE, true);
    clockAlignedDataIntervalInt  = declareConfiguration<int>("ClockAlignedDataInterval", 0);
    registerConfigurationValidator("ClockAlignedDataInterval", VALIDATE_UNSIGNED_INT);

    auto stopTxnAlignedDataString = declareConfiguration<const char*>("StopTxnAlignedData", "");

    meterValuesInTxOnlyBool = declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "MeterValuesInTxOnly", true);
    stopTxnDataCapturePeriodicBool = declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "StopTxnDataCapturePeriodic", false);

    transactionMessageAttemptsInt = declareConfiguration<int>("TransactionMessageAttempts", 3);
    transactionMessageRetryIntervalInt = declareConfiguration<int>("TransactionMessageRetryInterval", 60);

    sampledDataBuilder = std::unique_ptr<MeterValueBuilder>(new MeterValueBuilder(samplers, meterValuesSampledDataString));
    alignedDataBuilder = std::unique_ptr<MeterValueBuilder>(new MeterValueBuilder(samplers, meterValuesAlignedDataString));
    stopTxnSampledDataBuilder = std::unique_ptr<MeterValueBuilder>(new MeterValueBuilder(samplers, stopTxnSampledDataString));
    stopTxnAlignedDataBuilder = std::unique_ptr<MeterValueBuilder>(new MeterValueBuilder(samplers, stopTxnAlignedDataString));
}

void MeteringServiceEvse::loop() {

    bool txBreak = false;
    if (model.getConnector(connectorId)) {
        auto &curTx = model.getConnector(connectorId)->getTransaction();
        txBreak = (curTx && curTx->isRunning()) != trackTxRunning;
        trackTxRunning = (curTx && curTx->isRunning());
    }

    if (txBreak) {
        lastSampleTime = mocpp_tick_ms();
    }

    if (model.getConnector(connectorId)) {
        if (transaction != model.getConnector(connectorId)->getTransaction()) {
            transaction = model.getConnector(connectorId)->getTransaction();
        }

        if (transaction && transaction->isRunning() && !transaction->isSilent()) {
            //check during transaction

            if (!stopTxnData || stopTxnData->getTxNr() != transaction->getTxNr()) {
                MO_DBG_WARN("reload stopTxnData, %s, for tx-%u-%u", stopTxnData ? "replace" : "first time", connectorId, transaction->getTxNr());
                //reload (e.g. after power cut during transaction)
                stopTxnData = meterStore.getTxMeterData(*stopTxnSampledDataBuilder, transaction.get());
            }
        } else {
            //check outside of transaction

            if (connectorId != 0 && meterValuesInTxOnlyBool->getBool()) {
                //don't take any MeterValues outside of transactions on connectorIds other than 0
                return;
            }
        }
    }

    if (clockAlignedDataIntervalInt->getInt() >= 1 && model.getClock().now() >= MIN_TIME) {

        auto& timestampNow = model.getClock().now();
        auto dt = nextAlignedTime - timestampNow;
        if (dt < 0 ||                              //normal case: interval elapsed
                dt > clockAlignedDataIntervalInt->getInt()) {   //special case: clock has been adjusted or first run

            MO_DBG_DEBUG("Clock aligned measurement %ds: %s", dt,
                abs(dt) <= 60 ?
                "in time (tolerance <= 60s)" : "off, e.g. because of first run. Ignore");
            if (abs(dt) <= 60) { //is measurement still "clock-aligned"?

                if (auto alignedMeterValue = alignedDataBuilder->takeSample(model.getClock().now(), ReadingContext_SampleClock)) {
                    if (meterData.size() >= MO_METERVALUES_CACHE_MAXSIZE) {
                        MO_DBG_INFO("MeterValue cache full. Drop old MV");
                        meterData.erase(meterData.begin());
                    }
                    alignedMeterValue->setOpNr(context.getRequestQueue().getNextOpNr());
                    if (transaction) {
                        alignedMeterValue->setTxNr(transaction->getTxNr());
                    }
                    meterData.push_back(std::move(alignedMeterValue));
                }

                if (stopTxnData) {
                    auto alignedStopTx = stopTxnAlignedDataBuilder->takeSample(model.getClock().now(), ReadingContext_SampleClock);
                    if (alignedStopTx) {
                        stopTxnData->addTxData(std::move(alignedStopTx));
                    }
                }
            }

            Timestamp midnightBase = Timestamp(2010,0,0,0,0,0);
            auto intervall = timestampNow - midnightBase;
            intervall %= 3600 * 24;
            Timestamp midnight = timestampNow - intervall;
            intervall += clockAlignedDataIntervalInt->getInt();
            if (intervall >= 3600 * 24) {
                //next measurement is tomorrow; set to precisely 00:00 
                nextAlignedTime = midnight;
                nextAlignedTime += 3600 * 24;
            } else {
                intervall /= clockAlignedDataIntervalInt->getInt();
                nextAlignedTime = midnight + (intervall * clockAlignedDataIntervalInt->getInt());
            }
        }
    }

    if (meterValueSampleIntervalInt->getInt() >= 1) {
        //record periodic tx data

        if (mocpp_tick_ms() - lastSampleTime >= (unsigned long) (meterValueSampleIntervalInt->getInt() * 1000)) {
            if (auto sampledMeterValue = sampledDataBuilder->takeSample(model.getClock().now(), ReadingContext_SamplePeriodic)) {
                if (meterData.size() >= MO_METERVALUES_CACHE_MAXSIZE) {
                    MO_DBG_INFO("MeterValue cache full. Drop old MV");
                    meterData.erase(meterData.begin());
                }
                sampledMeterValue->setOpNr(context.getRequestQueue().getNextOpNr());
                if (transaction) {
                    sampledMeterValue->setTxNr(transaction->getTxNr());
                }
                meterData.push_back(std::move(sampledMeterValue));
            }

            if (stopTxnData && stopTxnDataCapturePeriodicBool->getBool()) {
                auto sampleStopTx = stopTxnSampledDataBuilder->takeSample(model.getClock().now(), ReadingContext_SamplePeriodic);
                if (sampleStopTx) {
                    stopTxnData->addTxData(std::move(sampleStopTx));
                }
            }
            lastSampleTime = mocpp_tick_ms();
        }
    }
}

std::unique_ptr<Operation> MeteringServiceEvse::takeTriggeredMeterValues() {

    auto sample = sampledDataBuilder->takeSample(model.getClock().now(), ReadingContext_Trigger);

    if (!sample) {
        return nullptr;
    }

    std::shared_ptr<Transaction> transaction = nullptr;
    if (model.getConnector(connectorId)) {
        transaction = model.getConnector(connectorId)->getTransaction();
    }

    auto msg = std::unique_ptr<MeterValues>(new MeterValues(model, std::move(sample), connectorId, transaction));
    auto meterValues = makeRequest(std::move(msg));
    meterValues->setTimeout(120000);
    return meterValues;
}

void MeteringServiceEvse::addMeterValueSampler(std::unique_ptr<SampledValueSampler> meterValueSampler) {
    if (!strcmp(meterValueSampler->getProperties().getMeasurand(), "Energy.Active.Import.Register")) {
        energySamplerIndex = samplers.size();
    }
    samplers.push_back(std::move(meterValueSampler));
}

std::unique_ptr<SampledValue> MeteringServiceEvse::readTxEnergyMeter(ReadingContext model) {
    if (energySamplerIndex >= 0 && (size_t) energySamplerIndex < samplers.size()) {
        return samplers[energySamplerIndex]->takeValue(model);
    } else {
        MO_DBG_DEBUG("Called readTxEnergyMeter(), but no energySampler or handling strategy set");
        return nullptr;
    }
}

void MeteringServiceEvse::beginTxMeterData(Transaction *transaction) {
    if (!stopTxnData || stopTxnData->getTxNr() != transaction->getTxNr()) {
        stopTxnData = meterStore.getTxMeterData(*stopTxnSampledDataBuilder, transaction);
    }

    if (stopTxnData) {
        auto sampleTxBegin = stopTxnSampledDataBuilder->takeSample(model.getClock().now(), ReadingContext_TransactionBegin);
        if (sampleTxBegin) {
            stopTxnData->addTxData(std::move(sampleTxBegin));
        }
    }
}

std::shared_ptr<TransactionMeterData> MeteringServiceEvse::endTxMeterData(Transaction *transaction) {
    if (!stopTxnData || stopTxnData->getTxNr() != transaction->getTxNr()) {
        stopTxnData = meterStore.getTxMeterData(*stopTxnSampledDataBuilder, transaction);
    }

    if (stopTxnData) {
        auto sampleTxEnd = stopTxnSampledDataBuilder->takeSample(model.getClock().now(), ReadingContext_TransactionEnd);
        if (sampleTxEnd) {
            stopTxnData->addTxData(std::move(sampleTxEnd));
        }
    }

    return std::move(stopTxnData);
}

void MeteringServiceEvse::abortTxMeterData() {
    stopTxnData.reset();
}

std::shared_ptr<TransactionMeterData> MeteringServiceEvse::getStopTxMeterData(Transaction *transaction) {
    auto txData = meterStore.getTxMeterData(*stopTxnSampledDataBuilder, transaction);

    if (!txData) {
        MO_DBG_ERR("could not create TxData");
        return nullptr;
    }

    return txData;
}

bool MeteringServiceEvse::removeTxMeterData(unsigned int txNr) {
    return meterStore.remove(connectorId, txNr);
}

bool MeteringServiceEvse::existsSampler(const char *measurand, size_t len) {
    for (size_t i = 0; i < samplers.size(); i++) {
        if (strlen(samplers[i]->getProperties().getMeasurand()) == len &&
                !strncmp(measurand, samplers[i]->getProperties().getMeasurand(), len)) {
            return true;
        }
    }

    return false;
}

unsigned int MeteringServiceEvse::getFrontRequestOpNr() {
    if (!meterDataFront && !meterData.empty()) {
        MO_DBG_DEBUG("advance MV front");
        meterDataFront = std::move(meterData.front());
        meterData.erase(meterData.begin());
    }
    if (meterDataFront) {
        return meterDataFront->getOpNr();
    }
    return NoOperation;
}

std::unique_ptr<Request> MeteringServiceEvse::fetchFrontRequest() {

    if (!meterDataFront) {
        return nullptr;
    }

    if ((int)meterDataFront->getAttemptNr() >= transactionMessageAttemptsInt->getInt()) {
        MO_DBG_WARN("exceeded TransactionMessageAttempts. Discard MeterValue");
        meterDataFront.reset();
        return nullptr;
    }

    if (mocpp_tick_ms() - meterDataFront->getAttemptTime() < meterDataFront->getAttemptNr() * (unsigned long)(std::max(0, transactionMessageRetryIntervalInt->getInt())) * 1000UL) {
        return nullptr;
    }

    meterDataFront->advanceAttemptNr();
    meterDataFront->setAttemptTime(mocpp_tick_ms());

    //fetch tx for meterValue
    std::shared_ptr<Transaction> tx;
    if (meterDataFront->getTxNr() >= 0) {
        tx = model.getTransactionStore()->getTransaction(connectorId, meterDataFront->getTxNr());
    }

    //discard MV if it belongs to silent tx
    if (tx && tx->isSilent()) {
        MO_DBG_DEBUG("Drop MeterValue belonging to silent tx");
        meterDataFront.reset();
        return nullptr;
    }

    auto meterValues = makeRequest(new MeterValues(model, meterDataFront.get(), connectorId, tx));
    meterValues->setOnReceiveConfListener([this] (JsonObject) {
        //operation success
        MO_DBG_DEBUG("drop MV front");
        meterDataFront.reset();
    });

    return meterValues;
}

MeteringService::MeteringService(Context& context, int numConn, std::shared_ptr<FilesystemAdapter> filesystem)
      : MemoryManaged("v16.Metering.MeteringService"), context(context), meterStore(filesystem), connectors(makeVector<std::unique_ptr<MeteringServiceEvse>>(getMemoryTag())) {

    //set factory defaults for Metering-related config keys
    declareConfiguration<const char*>("MeterValuesSampledData", "Energy.Active.Import.Register,Power.Active.Import");
    declareConfiguration<const char*>("StopTxnSampledData", "");
    declareConfiguration<const char*>("MeterValuesAlignedData", "Energy.Active.Import.Register,Power.Active.Import");
    declareConfiguration<const char*>("StopTxnAlignedData", "");
    
    connectors.reserve(numConn);
    for (int i = 0; i < numConn; i++) {
        connectors.emplace_back(new MeteringServiceEvse(context, i, meterStore));
    }

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
            for (size_t cId = 0; cId < connectors.size(); cId++) {
                if (connectors[cId]->existsSampler(l, (size_t) (r - l))) {
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

    registerConfigurationValidator("MeterValuesSampledData", validateSelectString);
    registerConfigurationValidator("StopTxnSampledData", validateSelectString);
    registerConfigurationValidator("MeterValuesAlignedData", validateSelectString);
    registerConfigurationValidator("StopTxnAlignedData", validateSelectString);
    registerConfigurationValidator("MeterValueSampleInterval", VALIDATE_UNSIGNED_INT);
    registerConfigurationValidator("ClockAlignedDataInterval", VALIDATE_UNSIGNED_INT);

    /*
     * Register further message handlers to support echo mode: when this library
     * is connected with a WebSocket echo server, let it reply to its own requests.
     * Mocking an OCPP Server on the same device makes running (unit) tests easier.
     */
    context.getOperationRegistry().registerOperation("MeterValues", [this] () {
        return new Ocpp16::MeterValues(this->context.getModel());});
}

void MeteringService::loop(){
    for (unsigned int i = 0; i < connectors.size(); i++){
        connectors[i]->loop();
    }
}
