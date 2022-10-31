// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Tasks/Metering/ConnectorMeterValuesRecorder.h>
#include <ArduinoOcpp/Tasks/Metering/MeterStore.h>
#include <ArduinoOcpp/Tasks/Transactions/Transaction.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/MessagesV16/MeterValues.h>
#include <ArduinoOcpp/Platform.h>
#include <ArduinoOcpp/Debug.h>

using namespace ArduinoOcpp;
using namespace ArduinoOcpp::Ocpp16;

ConnectorMeterValuesRecorder::ConnectorMeterValuesRecorder(OcppModel& context, int connectorId, MeterStore& meterStore)
        : context(context), connectorId{connectorId}, meterStore{meterStore} {

    auto MeterValuesSampledData = declareConfiguration<const char*>(
        "MeterValuesSampledData",
        "Energy.Active.Import.Register,Power.Active.Import",
        CONFIGURATION_FN,
        true,true,true,false
    );
    MeterValuesSampledDataMaxLength = declareConfiguration("MeterValuesSampledDataMaxLength", 4, CONFIGURATION_VOLATILE, false, true, false, false);
    MeterValueSampleInterval = declareConfiguration("MeterValueSampleInterval", 60);
    
    auto StopTxnSampledData = declareConfiguration<const char*>(
        "StopTxnSampledData",
        "",
        CONFIGURATION_FN,
        true,true,true,false
    );
    StopTxnSampledDataMaxLength = declareConfiguration("StopTxnSampledDataMaxLength", 4, CONFIGURATION_VOLATILE, false, true, false, false);
    
    auto MeterValuesAlignedData = declareConfiguration<const char*>(
        "MeterValuesAlignedData",
        "Energy.Active.Import.Register,Power.Active.Import",
        CONFIGURATION_FN,
        true,true,true,false
    );
    MeterValuesAlignedDataMaxLength = declareConfiguration("MeterValuesAlignedDataMaxLength", 4, CONFIGURATION_VOLATILE, false, true, false, false);
    ClockAlignedDataInterval  = declareConfiguration("ClockAlignedDataInterval", 0);
    
    auto StopTxnAlignedData = declareConfiguration<const char*>(
        "StopTxnAlignedData",
        "",
        CONFIGURATION_FN,
        true,true,true,false
    );
    StopTxnAlignedDataMaxLength = declareConfiguration("StopTxnAlignedDataMaxLength", 4, CONFIGURATION_VOLATILE, false, true, false, false);
    
    sampledDataBuilder = std::unique_ptr<MeterValueBuilder>(new MeterValueBuilder(samplers, MeterValuesSampledData));
    alignedDataBuilder = std::unique_ptr<MeterValueBuilder>(new MeterValueBuilder(samplers, MeterValuesAlignedData));
    stopTxnSampledDataBuilder = std::unique_ptr<MeterValueBuilder>(new MeterValueBuilder(samplers, StopTxnSampledData));
    stopTxnAlignedDataBuilder = std::unique_ptr<MeterValueBuilder>(new MeterValueBuilder(samplers, StopTxnAlignedData));
}

OcppMessage *ConnectorMeterValuesRecorder::loop() {

    Transaction *transaction = nullptr;

    if (context.getConnectorStatus(connectorId) && context.getConnectorStatus(connectorId)->getTransaction()) {
        transaction = context.getConnectorStatus(connectorId)->getTransaction().get();

        if (transaction->isRunning() && (!stopTxnData || stopTxnData->getTxNr() != transaction->getTxNr())) {
            AO_DBG_WARN("reload stopTxnData");
            //reload (e.g. after power cut during transaction)
            stopTxnData = meterStore.getTxMeterData(*stopTxnSampledDataBuilder, connectorId, transaction->getTxNr());
        }
    }

    if (*ClockAlignedDataInterval >= 1) {

        if (alignedData.size() >= (size_t) *MeterValuesAlignedDataMaxLength) {
            auto meterValues = new MeterValues(std::move(alignedData), connectorId);
            alignedData.clear();
            return meterValues;
        }

        auto& timestampNow = context.getOcppTime().getOcppTimestampNow();
        auto dt = nextAlignedTime - timestampNow;
        if (dt <= 0 ||                              //normal case: interval elapsed
                dt > *ClockAlignedDataInterval) {   //special case: clock has been adjusted or first run

            AO_DBG_DEBUG("Clock aligned measurement %ds: %s", dt,
                abs(dt) <= 60 ?
                "in time (tolerance <= 60s)" : "off, e.g. because of first run. Ignore");
            if (abs(dt) <= 60) { //is measurement still "clock-aligned"?
                auto alignedMeterValues = alignedDataBuilder->takeSample(context.getOcppTime().getOcppTimestampNow(), ReadingContext::SampleClock);
                if (alignedMeterValues) {
                    alignedData.push_back(std::move(alignedMeterValues));
                }

                if (stopTxnData) {
                    auto alignedStopTx = stopTxnAlignedDataBuilder->takeSample(context.getOcppTime().getOcppTimestampNow(), ReadingContext::SampleClock);
                    if (alignedStopTx) {
                        stopTxnData->addTxData(std::move(alignedStopTx));
                    }
                }

            }
            
            OcppTimestamp midnightBase = OcppTimestamp(2010,0,0,0,0,0);
            auto intervall = timestampNow - midnightBase;
            intervall %= 3600 * 24;
            OcppTimestamp midnight = timestampNow - intervall;
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
    } else {
        alignedData.clear();
    }

    if (*MeterValueSampleInterval >= 1) {
        //record periodic tx data

        if (sampledData.size() >= (size_t) *MeterValuesSampledDataMaxLength) {
            auto meterValues = new MeterValues(std::move(sampledData), connectorId);
            sampledData.clear();
            return meterValues;
        }

        if ((transaction && transaction->isRunning()) != trackTxRunning) {
            //transaction break

            trackTxRunning = (transaction && transaction->isRunning());

            std::shared_ptr<Transaction> transaction_sp = nullptr;
            if (context.getConnectorStatus(connectorId)) {
                transaction_sp = context.getConnectorStatus(connectorId)->getTransaction();
            }

            MeterValues *meterValues = nullptr;
            if (!sampledData.empty()) {
                meterValues = new MeterValues(std::move(sampledData), connectorId, transaction_sp);
                sampledData.clear();
            }
            lastSampleTime = ao_tick_ms();
            return meterValues;
        }

        if (ao_tick_ms() - lastSampleTime >= (ulong) (*MeterValueSampleInterval * 1000)) {
            auto sampleMeterValues = sampledDataBuilder->takeSample(context.getOcppTime().getOcppTimestampNow(), ReadingContext::SamplePeriodic);
            if (sampleMeterValues) {
                sampledData.push_back(std::move(sampleMeterValues));
            }

            if (stopTxnData) {
                auto sampleStopTx = stopTxnSampledDataBuilder->takeSample(context.getOcppTime().getOcppTimestampNow(), ReadingContext::SamplePeriodic);
                if (sampleStopTx) {
                    stopTxnData->addTxData(std::move(sampleStopTx));
                }
            }
            lastSampleTime = ao_tick_ms();
        }   

    } else {
        sampledData.clear();
    }

    return nullptr; //successful method completition. Currently there is no reason to send a MeterValues Msg.
}

OcppMessage *ConnectorMeterValuesRecorder::takeTriggeredMeterValues() {

    auto sample = sampledDataBuilder->takeSample(context.getOcppTime().getOcppTimestampNow(), ReadingContext::Trigger);

    if (!sample) {
        return nullptr;
    }

    decltype(sampledData) mv_now;
    mv_now.push_back(std::move(sample));

    return new MeterValues(std::move(mv_now), connectorId);
}

void ConnectorMeterValuesRecorder::setPowerSampler(PowerSampler ps){
    this->powerSampler = ps;
}

void ConnectorMeterValuesRecorder::setEnergySampler(EnergySampler es){
    this->energySampler = es;
}

void ConnectorMeterValuesRecorder::addMeterValueSampler(std::unique_ptr<SampledValueSampler> meterValueSampler) {
    if (!meterValueSampler->getProperties().getMeasurand().compare("Energy.Active.Import.Register")) {
        energySamplerIndex = samplers.size();
    }
    samplers.push_back(std::move(meterValueSampler));
}

std::unique_ptr<SampledValue> ConnectorMeterValuesRecorder::readTxEnergyMeter(ReadingContext reason) {
    if (energySamplerIndex >= 0 && (size_t) energySamplerIndex < samplers.size()) {
        return samplers[energySamplerIndex]->takeValue(reason);
    } else {
        AO_DBG_DEBUG("Called readTxEnergyMeter(), but no energySampler or handling strategy set");
        return nullptr;
    }
}

void ConnectorMeterValuesRecorder::beginTxMeterData(Transaction *transaction) {
    if (!stopTxnData || stopTxnData->getTxNr() != transaction->getTxNr()) {
        stopTxnData = meterStore.getTxMeterData(*stopTxnSampledDataBuilder, connectorId, transaction->getTxNr());
    }

    if (stopTxnData) {
        auto sampleTxBegin = stopTxnSampledDataBuilder->takeSample(context.getOcppTime().getOcppTimestampNow(), ReadingContext::TransactionBegin);
        if (sampleTxBegin) {
            stopTxnData->addTxData(std::move(sampleTxBegin));
        }
    }
}

std::shared_ptr<TransactionMeterData> ConnectorMeterValuesRecorder::endTxMeterData(Transaction *transaction) {
    if (!stopTxnData || stopTxnData->getTxNr() != transaction->getTxNr()) {
        stopTxnData = meterStore.getTxMeterData(*stopTxnSampledDataBuilder, connectorId, transaction->getTxNr());
    }

    if (stopTxnData) {
        auto sampleTxEnd = stopTxnSampledDataBuilder->takeSample(context.getOcppTime().getOcppTimestampNow(), ReadingContext::TransactionEnd);
        if (sampleTxEnd) {
            stopTxnData->addTxData(std::move(sampleTxEnd));
        }
    }

    return std::move(stopTxnData);
}

std::shared_ptr<TransactionMeterData> ConnectorMeterValuesRecorder::getStopTxMeterData(Transaction *transaction) {

    auto txData = meterStore.getTxMeterData(*stopTxnSampledDataBuilder, connectorId, transaction->getTxNr());

    if (!txData) {
        AO_DBG_ERR("could not create TxData");
        return nullptr;
    }

    return txData;
}
