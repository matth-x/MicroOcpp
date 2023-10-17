// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Model/Metering/MeteringConnector.h>
#include <MicroOcpp/Model/Metering/MeterStore.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Operations/MeterValues.h>
#include <MicroOcpp/Platform.h>
#include <MicroOcpp/Debug.h>

#include <cstddef>
#include <cinttypes>

using namespace MicroOcpp;
using namespace MicroOcpp::Ocpp16;

MeteringConnector::MeteringConnector(Model& model, int connectorId, MeterStore& meterStore)
        : model(model), connectorId{connectorId}, meterStore(meterStore) {

    auto meterValuesSampledDataString = declareConfiguration<const char*>("MeterValuesSampledData", "");
    declareConfiguration<int>("MeterValuesSampledDataMaxLength", 8, CONFIGURATION_VOLATILE, true);
    meterValueCacheSizeInt = declareConfiguration<int>(MO_CONFIG_EXT_PREFIX "MeterValueCacheSize", 1);
    meterValueSampleIntervalInt = declareConfiguration<int>("MeterValueSampleInterval", 60);
    
    auto stopTxnSampledDataString = declareConfiguration<const char*>("StopTxnSampledData", "");
    declareConfiguration<int>("StopTxnSampledDataMaxLength", 8, CONFIGURATION_VOLATILE, true);
    
    auto meterValuesAlignedDataString = declareConfiguration<const char*>("MeterValuesAlignedData", "");
    declareConfiguration<int>("MeterValuesAlignedDataMaxLength", 8, CONFIGURATION_VOLATILE, true);
    clockAlignedDataIntervalInt  = declareConfiguration<int>("ClockAlignedDataInterval", 0);
    
    auto stopTxnAlignedDataString = declareConfiguration<const char*>("StopTxnAlignedData", "");

    meterValuesInTxOnlyBool = declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "MeterValuesInTxOnly", true);
    stopTxnDataCapturePeriodicBool = declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "StopTxnDataCapturePeriodic", false);

    sampledDataBuilder = std::unique_ptr<MeterValueBuilder>(new MeterValueBuilder(samplers, meterValuesSampledDataString));
    alignedDataBuilder = std::unique_ptr<MeterValueBuilder>(new MeterValueBuilder(samplers, meterValuesAlignedDataString));
    stopTxnSampledDataBuilder = std::unique_ptr<MeterValueBuilder>(new MeterValueBuilder(samplers, stopTxnSampledDataString));
    stopTxnAlignedDataBuilder = std::unique_ptr<MeterValueBuilder>(new MeterValueBuilder(samplers, stopTxnAlignedDataString));
}

std::unique_ptr<Operation> MeteringConnector::loop() {

    bool txBreak = false;
    if (model.getConnector(connectorId)) {
        auto &curTx = model.getConnector(connectorId)->getTransaction();
        txBreak = (curTx && curTx->isRunning()) != trackTxRunning;
        trackTxRunning = (curTx && curTx->isRunning());
    }

    if (txBreak) {
        lastSampleTime = mocpp_tick_ms();
    }

    if ((txBreak || meterData.size() >= (size_t) meterValueCacheSizeInt->getInt()) && !meterData.empty()) {
        auto meterValues = std::unique_ptr<MeterValues>(new MeterValues(std::move(meterData), connectorId, transaction));
        meterData.clear();
        return std::move(meterValues);
    }

    if (model.getConnector(connectorId)) {
        if (transaction != model.getConnector(connectorId)->getTransaction()) {
            transaction = model.getConnector(connectorId)->getTransaction();
        }
        
        if (transaction && transaction->isRunning() && !transaction->isSilent()) {
            //check during transaction

            if (!stopTxnData || stopTxnData->getTxNr() != transaction->getTxNr()) {
                MO_DBG_WARN("reload stopTxnData");
                //reload (e.g. after power cut during transaction)
                stopTxnData = meterStore.getTxMeterData(*stopTxnSampledDataBuilder, transaction.get());
            }
        } else {
            //check outside of transaction

            if (connectorId != 0 && meterValuesInTxOnlyBool->getBool()) {
                //don't take any MeterValues outside of transactions on connectorIds other than 0
                meterData.clear();
                return nullptr;
            }
        }
    }

    if (clockAlignedDataIntervalInt->getInt() >= 1) {

        auto& timestampNow = model.getClock().now();
        auto dt = nextAlignedTime - timestampNow;
        if (dt <= 0 ||                              //normal case: interval elapsed
                dt > clockAlignedDataIntervalInt->getInt()) {   //special case: clock has been adjusted or first run

            MO_DBG_DEBUG("Clock aligned measurement %" PRId32 "s: %s", dt,
                abs(dt) <= 60 ?
                "in time (tolerance <= 60s)" : "off, e.g. because of first run. Ignore");
            if (abs(dt) <= 60) { //is measurement still "clock-aligned"?
                auto alignedMeterValues = alignedDataBuilder->takeSample(model.getClock().now(), ReadingContext::SampleClock);
                if (alignedMeterValues) {
                    meterData.push_back(std::move(alignedMeterValues));
                }

                if (stopTxnData) {
                    auto alignedStopTx = stopTxnAlignedDataBuilder->takeSample(model.getClock().now(), ReadingContext::SampleClock);
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
            auto sampleMeterValues = sampledDataBuilder->takeSample(model.getClock().now(), ReadingContext::SamplePeriodic);
            if (sampleMeterValues) {
                meterData.push_back(std::move(sampleMeterValues));
            }

            if (stopTxnData && stopTxnDataCapturePeriodicBool->getBool()) {
                auto sampleStopTx = stopTxnSampledDataBuilder->takeSample(model.getClock().now(), ReadingContext::SamplePeriodic);
                if (sampleStopTx) {
                    stopTxnData->addTxData(std::move(sampleStopTx));
                }
            }
            lastSampleTime = mocpp_tick_ms();
        }   
    }

    if (clockAlignedDataIntervalInt->getInt() < 1 && meterValueSampleIntervalInt->getInt() < 1) {
        meterData.clear();
    }

    return nullptr; //successful method completition. Currently there is no reason to send a MeterValues Msg.
}

std::unique_ptr<Operation> MeteringConnector::takeTriggeredMeterValues() {

    auto sample = sampledDataBuilder->takeSample(model.getClock().now(), ReadingContext::Trigger);

    if (!sample) {
        return nullptr;
    }

    decltype(meterData) mv_now;
    mv_now.push_back(std::move(sample));

    std::shared_ptr<Transaction> transaction = nullptr;
    if (model.getConnector(connectorId)) {
        transaction = model.getConnector(connectorId)->getTransaction();
    }

    return std::unique_ptr<MeterValues>(new MeterValues(std::move(mv_now), connectorId, transaction));
}

void MeteringConnector::addMeterValueSampler(std::unique_ptr<SampledValueSampler> meterValueSampler) {
    if (!meterValueSampler->getProperties().getMeasurand().compare("Energy.Active.Import.Register")) {
        energySamplerIndex = samplers.size();
    }
    samplers.push_back(std::move(meterValueSampler));
}

std::unique_ptr<SampledValue> MeteringConnector::readTxEnergyMeter(ReadingContext model) {
    if (energySamplerIndex >= 0 && (size_t) energySamplerIndex < samplers.size()) {
        return samplers[energySamplerIndex]->takeValue(model);
    } else {
        MO_DBG_DEBUG("Called readTxEnergyMeter(), but no energySampler or handling strategy set");
        return nullptr;
    }
}

void MeteringConnector::beginTxMeterData(Transaction *transaction) {
    if (!stopTxnData || stopTxnData->getTxNr() != transaction->getTxNr()) {
        stopTxnData = meterStore.getTxMeterData(*stopTxnSampledDataBuilder, transaction);
    }

    if (stopTxnData) {
        auto sampleTxBegin = stopTxnSampledDataBuilder->takeSample(model.getClock().now(), ReadingContext::TransactionBegin);
        if (sampleTxBegin) {
            stopTxnData->addTxData(std::move(sampleTxBegin));
        }
    }
}

std::shared_ptr<TransactionMeterData> MeteringConnector::endTxMeterData(Transaction *transaction) {
    if (!stopTxnData || stopTxnData->getTxNr() != transaction->getTxNr()) {
        stopTxnData = meterStore.getTxMeterData(*stopTxnSampledDataBuilder, transaction);
    }

    if (stopTxnData) {
        auto sampleTxEnd = stopTxnSampledDataBuilder->takeSample(model.getClock().now(), ReadingContext::TransactionEnd);
        if (sampleTxEnd) {
            stopTxnData->addTxData(std::move(sampleTxEnd));
        }
    }

    return std::move(stopTxnData);
}

std::shared_ptr<TransactionMeterData> MeteringConnector::getStopTxMeterData(Transaction *transaction) {
    auto txData = meterStore.getTxMeterData(*stopTxnSampledDataBuilder, transaction);

    if (!txData) {
        MO_DBG_ERR("could not create TxData");
        return nullptr;
    }

    return txData;
}

bool MeteringConnector::existsSampler(const char *measurand, size_t len) {
    for (size_t i = 0; i < samplers.size(); i++) {
        if (samplers[i]->getProperties().getMeasurand().length() == len &&
                !strncmp(measurand, samplers[i]->getProperties().getMeasurand().c_str(), len)) {
            return true;
        }
    }

    return false;
}
