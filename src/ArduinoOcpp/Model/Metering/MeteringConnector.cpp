// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Model/Metering/MeteringConnector.h>
#include <ArduinoOcpp/Model/Metering/MeterStore.h>
#include <ArduinoOcpp/Model/Transactions/Transaction.h>
#include <ArduinoOcpp/Model/Model.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Operations/MeterValues.h>
#include <ArduinoOcpp/Platform.h>
#include <ArduinoOcpp/Debug.h>

#include <cstddef>
#include <cinttypes>

using namespace ArduinoOcpp;
using namespace ArduinoOcpp::Ocpp16;

MeteringConnector::MeteringConnector(Model& model, int connectorId, MeterStore& meterStore)
        : model(model), connectorId{connectorId}, meterStore(meterStore) {

    auto MeterValuesSampledData = declareConfiguration<const char*>(
        "MeterValuesSampledData",
        "Energy.Active.Import.Register,Power.Active.Import",
        CONFIGURATION_FN,
        true,true,true,false
    );
    declareConfiguration<int>("MeterValuesSampledDataMaxLength", 8, CONFIGURATION_VOLATILE, false, true, false, false);
    MeterValueCacheSize = declareConfiguration("AO_MeterValueCacheSize", 1, CONFIGURATION_FN, true, true, true, false);
    MeterValueSampleInterval = declareConfiguration("MeterValueSampleInterval", 60);
    
    auto StopTxnSampledData = declareConfiguration<const char*>(
        "StopTxnSampledData",
        "",
        CONFIGURATION_FN,
        true,true,true,false
    );
    declareConfiguration<int>("StopTxnSampledDataMaxLength", 8, CONFIGURATION_VOLATILE, false, true, false, false);
    
    auto MeterValuesAlignedData = declareConfiguration<const char*>(
        "MeterValuesAlignedData",
        "Energy.Active.Import.Register,Power.Active.Import",
        CONFIGURATION_FN,
        true,true,true,false
    );
    declareConfiguration<int>("MeterValuesAlignedDataMaxLength", 8, CONFIGURATION_VOLATILE, false, true, false, false);
    ClockAlignedDataInterval  = declareConfiguration("ClockAlignedDataInterval", 0);
    
    auto StopTxnAlignedData = declareConfiguration<const char*>(
        "StopTxnAlignedData",
        "",
        CONFIGURATION_FN,
        true,true,true,false
    );

    MeterValuesInTxOnly = declareConfiguration<bool>("AO_MeterValuesInTxOnly", true, CONFIGURATION_FN, true, true, true, false);
    StopTxnDataCapturePeriodic = declareConfiguration<bool>("AO_StopTxnDataCapturePeriodic", false, CONFIGURATION_FN, true, true, true, false);

    sampledDataBuilder = std::unique_ptr<MeterValueBuilder>(new MeterValueBuilder(samplers, MeterValuesSampledData));
    alignedDataBuilder = std::unique_ptr<MeterValueBuilder>(new MeterValueBuilder(samplers, MeterValuesAlignedData));
    stopTxnSampledDataBuilder = std::unique_ptr<MeterValueBuilder>(new MeterValueBuilder(samplers, StopTxnSampledData));
    stopTxnAlignedDataBuilder = std::unique_ptr<MeterValueBuilder>(new MeterValueBuilder(samplers, StopTxnAlignedData));

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
            for (size_t i = 0; i < samplers.size(); i++) {
                auto &measurand = samplers[i]->getProperties().getMeasurand();
                if ((std::ptrdiff_t) measurand.length() == r - l &&                              //same length
                        !strncmp(l, measurand.c_str(), measurand.length())) {   //same content
                    found = true;
                    break;
                }
            }
            if (!found) {
                isValid = false;
                AO_DBG_WARN("could not find metering device for %.*s", (int) (r - l), l);
                break;
            }
            l = r;
        }
        return isValid;
    };
    MeterValuesSampledData->setValidator(validateSelectString);
    StopTxnSampledData->setValidator(validateSelectString);
    MeterValuesAlignedData->setValidator(validateSelectString);
    StopTxnAlignedData->setValidator(validateSelectString);
}

std::unique_ptr<Operation> MeteringConnector::loop() {

    bool txBreak = false;
    if (model.getConnector(connectorId)) {
        auto &curTx = model.getConnector(connectorId)->getTransaction();
        txBreak = (curTx && curTx->isRunning()) != trackTxRunning;
        trackTxRunning = (curTx && curTx->isRunning());
    }

    if (txBreak) {
        lastSampleTime = ao_tick_ms();
    }

    if ((txBreak || meterData.size() >= (size_t) *MeterValueCacheSize) && !meterData.empty()) {
        auto meterValues = std::unique_ptr<MeterValues>(new MeterValues(std::move(meterData), connectorId, transaction));
        meterData.clear();
        return meterValues;
    }

    if (model.getConnector(connectorId)) {
        if (transaction != model.getConnector(connectorId)->getTransaction()) {
            transaction = model.getConnector(connectorId)->getTransaction();
        }
        
        if (transaction && transaction->isRunning() && !transaction->isSilent()) {
            //check during transaction

            if (!stopTxnData || stopTxnData->getTxNr() != transaction->getTxNr()) {
                AO_DBG_WARN("reload stopTxnData");
                //reload (e.g. after power cut during transaction)
                stopTxnData = meterStore.getTxMeterData(*stopTxnSampledDataBuilder, transaction.get());
            }
        } else {
            //check outside of transaction

            if (!MeterValuesInTxOnly || *MeterValuesInTxOnly) {
                //don't take any MeterValues outside of transactions
                meterData.clear();
                return nullptr;
            }
        }
    }

    if (*ClockAlignedDataInterval >= 1) {

        auto& timestampNow = model.getClock().now();
        auto dt = nextAlignedTime - timestampNow;
        if (dt <= 0 ||                              //normal case: interval elapsed
                dt > *ClockAlignedDataInterval) {   //special case: clock has been adjusted or first run

            AO_DBG_DEBUG("Clock aligned measurement %" PRId32 "s: %s", dt,
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
            intervall += *ClockAlignedDataInterval;
            if (intervall >= 3600 * 24) {
                //next measurement is tomorrow; set to precisely 00:00 
                nextAlignedTime = midnight;
                nextAlignedTime += 3600 * 24;
            } else {
                intervall /= *ClockAlignedDataInterval;
                nextAlignedTime = midnight + (intervall * *ClockAlignedDataInterval);
            }
        }
    }

    if (*MeterValueSampleInterval >= 1) {
        //record periodic tx data

        if (ao_tick_ms() - lastSampleTime >= (unsigned long) (*MeterValueSampleInterval * 1000)) {
            auto sampleMeterValues = sampledDataBuilder->takeSample(model.getClock().now(), ReadingContext::SamplePeriodic);
            if (sampleMeterValues) {
                meterData.push_back(std::move(sampleMeterValues));
            }

            if (stopTxnData && StopTxnDataCapturePeriodic && *StopTxnDataCapturePeriodic) {
                auto sampleStopTx = stopTxnSampledDataBuilder->takeSample(model.getClock().now(), ReadingContext::SamplePeriodic);
                if (sampleStopTx) {
                    stopTxnData->addTxData(std::move(sampleStopTx));
                }
            }
            lastSampleTime = ao_tick_ms();
        }   
    }

    if (*ClockAlignedDataInterval < 1 && *MeterValueSampleInterval < 1) {
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
        AO_DBG_DEBUG("Called readTxEnergyMeter(), but no energySampler or handling strategy set");
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
        AO_DBG_ERR("could not create TxData");
        return nullptr;
    }

    return txData;
}
